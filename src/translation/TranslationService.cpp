#include "translation/TranslationService.h"

#include "Version.h"
#include "core/Languages.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>

Q_LOGGING_CATEGORY(lcTranslate, "hypers.translate")

namespace hypers {

namespace {

bool isRetryableNetworkError(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::TooManyRedirectsError:
        return false;
    default:
        return true;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// TranslationReply
// ---------------------------------------------------------------------------

TranslationReply::TranslationReply(TranslationService* service, ITranslationProvider* provider,
                                   const TranslationRequest& request, QObject* parent)
    : QObject(parent)
    , m_service(service)
    , m_provider(provider)
    , m_request(request)
    , m_providerId(provider ? provider->id() : QString())
{
    m_elapsed.start();
    m_timeoutTimer.setSingleShot(true);
    m_retryTimer.setSingleShot(true);

    connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        if (m_networkReply) {
            m_timedOut = true;
            m_networkReply->abort(); // -> onNetworkFinished()
        }
    });
    connect(&m_retryTimer, &QTimer::timeout, this, &TranslationReply::sendCurrentBatch);
}

TranslationReply::~TranslationReply()
{
    if (m_networkReply) {
        m_networkReply->disconnect(this);
        m_networkReply->abort();
        m_networkReply->deleteLater();
    }
}

void TranslationReply::abort()
{
    if (m_finished)
        return;
    m_retryTimer.stop();
    m_timeoutTimer.stop();
    const QPointer<QNetworkReply> pending = m_networkReply;
    TranslationError error;
    error.kind = TranslationErrorKind::Canceled;
    error.message = QStringLiteral("Canceled by user");
    finishWithError(error);
    if (pending)
        pending->abort();
}

void TranslationReply::start()
{
    if (m_finished)
        return;

    TranslationError error;
    if (!m_service || !m_provider) {
        error.kind = TranslationErrorKind::NotConfigured;
        error.message = QStringLiteral("Unknown translation provider");
        finishWithError(error);
        return;
    }
    if (!m_provider->hasApiKey()) {
        error.kind = TranslationErrorKind::NotConfigured;
        error.message = QStringLiteral("%1 API key missing").arg(m_provider->displayName());
        finishWithError(error);
        return;
    }
    if (m_request.text.trimmed().isEmpty()) {
        error.kind = TranslationErrorKind::InvalidInput;
        error.message = QStringLiteral("Nothing to translate - the text is empty.");
        finishWithError(error);
        return;
    }
    if (m_request.text.size() > TranslationService::kMaxInputCharacters) {
        error.kind = TranslationErrorKind::InvalidInput;
        error.message = QStringLiteral("The text is too long (%1 characters, maximum %2).")
                            .arg(m_request.text.size())
                            .arg(TranslationService::kMaxInputCharacters);
        finishWithError(error);
        return;
    }
    if (!m_provider->supportsTargetLanguage(m_request.targetLanguage)) {
        error.kind = TranslationErrorKind::UnsupportedLanguage;
        error.message = QStringLiteral("%1 cannot translate into %2")
                            .arg(m_provider->displayName(), Languages::label(m_request.targetLanguage));
        finishWithError(error);
        return;
    }
    if (!m_provider->supportsSourceLanguage(m_request.sourceLanguage)) {
        qCInfo(lcTranslate) << m_providerId << "does not support source" << m_request.sourceLanguage
                            << "- falling back to auto-detect";
        m_request.sourceLanguage = QString::fromLatin1(Languages::kAuto);
    }

    if (m_request.text.size() <= TranslationService::kMaxCachedCharacters) {
        const QString key = TranslationService::cacheKey(m_providerId, m_request);
        if (const TranslationResult* cached = m_service->m_cache.object(key)) {
            TranslationResult copy = *cached;
            copy.fromCache = true;
            finishWith(copy);
            return;
        }
    }

    // Split into segments and group them into requests within provider limits.
    const int maxChars = m_provider->maxCharactersPerRequest();
    const int maxSegments = m_provider->maxSegmentsPerRequest();
    m_segments = TextSegmenter::split(m_request.text, maxChars);
    m_translatedCores = QStringList();
    m_translatedCores.reserve(m_segments.size());
    for (qsizetype i = 0; i < m_segments.size(); ++i)
        m_translatedCores.append(QString());

    std::vector<int> current;
    qsizetype currentChars = 0;
    for (int i = 0; i < static_cast<int>(m_segments.size()); ++i) {
        const qsizetype len = m_segments.at(i).core.size();
        if (len == 0)
            continue;
        if (!current.empty()
            && (static_cast<int>(current.size()) >= maxSegments || currentChars + len > maxChars)) {
            m_batches.push_back(std::move(current));
            current.clear();
            currentChars = 0;
        }
        current.push_back(i);
        currentChars += len;
    }
    if (!current.empty())
        m_batches.push_back(std::move(current));

    if (m_batches.empty()) {
        error.kind = TranslationErrorKind::InvalidInput;
        error.message = QStringLiteral("Nothing to translate - the text is empty.");
        finishWithError(error);
        return;
    }

    sendCurrentBatch();
}

void TranslationReply::sendCurrentBatch()
{
    if (m_finished)
        return;
    if (!m_service) {
        TranslationError error;
        error.kind = TranslationErrorKind::Canceled;
        error.message = QStringLiteral("Translation service shut down");
        finishWithError(error);
        return;
    }

    ++m_attempt;
    m_timedOut = false;

    QStringList cores;
    for (int index : m_batches.at(m_batchIndex))
        cores.append(m_segments.at(index).core);

    QNetworkRequest request = m_provider->buildRequest();
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("HYPERS-TRANSLATOR/%1 (Windows)").arg(QStringLiteral(HYPERS_VERSION_STRING)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    const QByteArray payload =
        m_provider->buildPayload(cores, m_request.sourceLanguage, m_request.targetLanguage);

    m_networkReply = m_service->m_network->post(request, payload);
    connect(m_networkReply, &QNetworkReply::finished, this, &TranslationReply::onNetworkFinished);
    m_timeoutTimer.start(m_service->requestTimeoutMs());
}

void TranslationReply::onNetworkFinished()
{
    QNetworkReply* reply = m_networkReply;
    m_networkReply = nullptr;
    m_timeoutTimer.stop();
    if (!reply)
        return;
    reply->deleteLater();
    if (m_finished)
        return; // aborted by the user

    const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int status = statusAttr.isValid() ? statusAttr.toInt() : 0;
    const QByteArray body = reply->readAll();

    if (m_timedOut) {
        TranslationError error;
        error.kind = TranslationErrorKind::Timeout;
        error.message = QStringLiteral("No response within %1 ms").arg(m_service ? m_service->requestTimeoutMs() : 0);
        handleBatchFailure(error);
        return;
    }

    if (status == 0) {
        TranslationError error;
        error.kind = TranslationErrorKind::Network;
        error.message = reply->errorString();
        if (!isRetryableNetworkError(reply->error())) {
            qCWarning(lcTranslate) << "Permanent network error:" << reply->error() << error.message;
            finishWithError(error);
            return;
        }
        handleBatchFailure(error);
        return;
    }

    ProviderResponse parsed = m_provider->parseResponse(status, body);
    if (!parsed.ok) {
        if (status == 429 || status == 503 || status == 529)
            parsed.error.retryAfterMs = RetryPolicy::parseRetryAfter(reply->rawHeader("Retry-After"));
        handleBatchFailure(parsed.error);
        return;
    }

    const std::vector<int>& batch = m_batches.at(m_batchIndex);
    if (parsed.translations.size() != static_cast<qsizetype>(batch.size())) {
        TranslationError error;
        error.kind = TranslationErrorKind::InvalidResponse;
        error.httpStatus = status;
        error.message = QStringLiteral("Expected %1 translations, got %2")
                            .arg(batch.size())
                            .arg(parsed.translations.size());
        finishWithError(error);
        return;
    }
    for (std::size_t i = 0; i < batch.size(); ++i)
        m_translatedCores[batch[i]] = parsed.translations.at(static_cast<qsizetype>(i));
    if (m_detectedSource.isEmpty())
        m_detectedSource = parsed.detectedSourceLanguage;

    ++m_batchIndex;
    m_attempt = 0;
    if (m_batchIndex < m_batches.size())
        sendCurrentBatch();
    else
        finishWithResult();
}

void TranslationReply::handleBatchFailure(const TranslationError& error)
{
    const int maxRetries = m_service ? m_service->retryPolicy().maxRetries : 0;
    const int retriesDone = m_attempt - 1;
    if (error.isRetryable() && retriesDone < maxRetries && m_service) {
        const int delay = m_service->retryPolicy().delayMs(m_attempt, error.retryAfterMs);
        qCInfo(lcTranslate).nospace() << m_providerId << ": attempt " << m_attempt << " failed ("
                                      << errorKindName(error.kind) << ", HTTP " << error.httpStatus << ": "
                                      << error.message << ") - retrying in " << delay << " ms";
        Q_EMIT retrying(m_attempt + 1, delay, error.userMessage());
        m_retryTimer.start(delay);
        return;
    }
    finishWithError(error);
}

void TranslationReply::finishWithResult()
{
    m_result.translatedText = TextSegmenter::join(m_segments, m_translatedCores);
    m_result.detectedSourceLanguage = m_detectedSource;
    m_result.providerId = m_providerId;
    m_result.fromCache = false;
    if (m_service)
        m_service->storeInCache(m_providerId, m_request, m_result);

    qCInfo(lcTranslate).nospace() << m_providerId << ": translated " << m_request.text.size() << " chars in "
                                  << m_batches.size() << " request(s), " << m_elapsed.elapsed() << " ms";
    m_finished = true;
    Q_EMIT finished();
}

void TranslationReply::finishWith(const TranslationResult& result)
{
    m_result = result;
    m_finished = true;
    Q_EMIT finished();
}

void TranslationReply::finishWithError(const TranslationError& error)
{
    if (m_finished)
        return;
    m_error = error;
    if (m_error.kind == TranslationErrorKind::None)
        m_error.kind = TranslationErrorKind::InvalidResponse;
    if (m_error.kind != TranslationErrorKind::Canceled) {
        qCWarning(lcTranslate).nospace() << m_providerId << ": failed - " << errorKindName(m_error.kind)
                                         << " (HTTP " << m_error.httpStatus << ") " << m_error.message;
    }
    m_finished = true;
    Q_EMIT finished();
}

// ---------------------------------------------------------------------------
// TranslationService
// ---------------------------------------------------------------------------

TranslationService::TranslationService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    qRegisterMetaType<TranslationResult>();
    qRegisterMetaType<TranslationError>();
}

TranslationService::~TranslationService() = default;

void TranslationService::registerProvider(std::unique_ptr<ITranslationProvider> provider)
{
    const QString id = provider->id();
    m_providers[id] = std::move(provider);
    if (m_activeProviderId.isEmpty())
        m_activeProviderId = id;
}

ITranslationProvider* TranslationService::provider(const QString& id) const
{
    const auto it = m_providers.find(id);
    return it == m_providers.end() ? nullptr : it->second.get();
}

QList<ITranslationProvider*> TranslationService::providers() const
{
    QList<ITranslationProvider*> list;
    for (const auto& [id, provider] : m_providers)
        list.append(provider.get());
    return list;
}

void TranslationService::setActiveProvider(const QString& id)
{
    if (!provider(id) || id == m_activeProviderId)
        return;
    m_activeProviderId = id;
    Q_EMIT activeProviderChanged(id);
}

TranslationReply* TranslationService::translate(const TranslationRequest& request, QObject* parent)
{
    return translateWith(m_activeProviderId, request, parent);
}

TranslationReply* TranslationService::translateWith(const QString& providerId, const TranslationRequest& request,
                                                    QObject* parent)
{
    auto* reply = new TranslationReply(this, provider(providerId), request, parent ? parent : this);
    reply->m_providerId = providerId;
    // Start on the next event-loop turn so callers can connect to finished() first.
    QMetaObject::invokeMethod(reply, &TranslationReply::start, Qt::QueuedConnection);
    return reply;
}

QString TranslationService::cacheKey(const QString& providerId, const TranslationRequest& request)
{
    const QChar sep(0x1F);
    return providerId + sep + request.sourceLanguage + sep + request.targetLanguage + sep + request.text;
}

void TranslationService::storeInCache(const QString& providerId, const TranslationRequest& request,
                                      const TranslationResult& result)
{
    // Large texts are not worth keeping in memory.
    if (request.text.size() > kMaxCachedCharacters)
        return;
    const qsizetype cost = qMax<qsizetype>(1, request.text.size() + result.translatedText.size());
    m_cache.insert(cacheKey(providerId, request), new TranslationResult(result), cost);
}

} // namespace hypers
