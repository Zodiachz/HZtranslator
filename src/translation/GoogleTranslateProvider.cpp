#include "translation/GoogleTranslateProvider.h"

#include "core/Languages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace hypers {

namespace {

// Collects every "reason" string found in a Google error payload
// (error.errors[].reason and error.details[].reason).
QStringList errorReasons(const QJsonObject& error)
{
    QStringList reasons;
    const QJsonArray errors = error.value(QStringLiteral("errors")).toArray();
    for (const QJsonValue& e : errors)
        reasons << e.toObject().value(QStringLiteral("reason")).toString();
    const QJsonArray details = error.value(QStringLiteral("details")).toArray();
    for (const QJsonValue& d : details)
        reasons << d.toObject().value(QStringLiteral("reason")).toString();
    reasons << error.value(QStringLiteral("status")).toString();
    reasons.removeAll(QString());
    return reasons;
}

bool containsAny(const QStringList& reasons, std::initializer_list<const char*> needles)
{
    for (const QString& reason : reasons) {
        for (const char* needle : needles) {
            if (reason.compare(QLatin1String(needle), Qt::CaseInsensitive) == 0)
                return true;
        }
    }
    return false;
}

} // namespace

bool GoogleTranslateProvider::supportsSourceLanguage(const QString& code) const
{
    if (code == QLatin1String(Languages::kAuto))
        return true;
    const LanguageInfo* lang = Languages::find(code);
    return lang && !lang->google.isEmpty();
}

bool GoogleTranslateProvider::supportsTargetLanguage(const QString& code) const
{
    const LanguageInfo* lang = Languages::find(code);
    return lang && !lang->google.isEmpty();
}

QNetworkRequest GoogleTranslateProvider::buildRequest() const
{
    QNetworkRequest request(QUrl(QStringLiteral("https://translation.googleapis.com/language/translate/v2")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    request.setRawHeader("X-Goog-Api-Key", m_apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");
    return request;
}

QByteArray GoogleTranslateProvider::buildPayload(const QStringList& segments,
                                                 const QString& sourceLanguage,
                                                 const QString& targetLanguage) const
{
    QJsonObject body;
    body.insert(QStringLiteral("q"), QJsonArray::fromStringList(segments));
    if (const LanguageInfo* target = Languages::find(targetLanguage))
        body.insert(QStringLiteral("target"), target->google);
    if (sourceLanguage != QLatin1String(Languages::kAuto)) {
        if (const LanguageInfo* source = Languages::find(sourceLanguage))
            body.insert(QStringLiteral("source"), source->google);
    }
    // "text" = no HTML entity escaping in the output.
    body.insert(QStringLiteral("format"), QStringLiteral("text"));
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

ProviderResponse GoogleTranslateProvider::parseResponse(int httpStatus, const QByteArray& body) const
{
    ProviderResponse response;
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (httpStatus != 200) {
        QString message;
        QStringList reasons;
        if (doc.isObject()) {
            const QJsonObject error = doc.object().value(QStringLiteral("error")).toObject();
            message = error.value(QStringLiteral("message")).toString();
            reasons = errorReasons(error);
        }
        response.error = errorForHttpStatus(httpStatus, message);

        // Quota / rate reasons first: Google pairs them with status PERMISSION_DENIED too.
        if (containsAny(reasons, {"dailyLimitExceeded", "quotaExceeded", "BILLING_DISABLED"})) {
            response.error.kind = TranslationErrorKind::QuotaExceeded;
        } else if (containsAny(reasons, {"userRateLimitExceeded", "rateLimitExceeded", "RATE_LIMIT_EXCEEDED",
                                         "RESOURCE_EXHAUSTED"})) {
            response.error.kind = TranslationErrorKind::RateLimited;
        } else if (containsAny(reasons, {"API_KEY_INVALID", "keyInvalid", "API_KEY_SERVICE_BLOCKED",
                                         "SERVICE_DISABLED", "accessNotConfigured", "PERMISSION_DENIED"})
                   || message.contains(QLatin1String("API key not valid"), Qt::CaseInsensitive)) {
            response.error.kind = TranslationErrorKind::Authentication;
        } else if (httpStatus == 400 && message.contains(QLatin1String("language"), Qt::CaseInsensitive)) {
            response.error.kind = TranslationErrorKind::UnsupportedLanguage;
        }
        return response;
    }

    if (!doc.isObject()) {
        response.error.kind = TranslationErrorKind::InvalidResponse;
        response.error.message = parseError.errorString();
        response.error.httpStatus = httpStatus;
        return response;
    }

    const QJsonArray translations =
        doc.object().value(QStringLiteral("data")).toObject().value(QStringLiteral("translations")).toArray();
    if (translations.isEmpty()) {
        response.error.kind = TranslationErrorKind::InvalidResponse;
        response.error.message = QStringLiteral("Missing 'data.translations' array");
        response.error.httpStatus = httpStatus;
        return response;
    }
    for (const QJsonValue& value : translations) {
        const QJsonObject item = value.toObject();
        response.translations.append(item.value(QStringLiteral("translatedText")).toString());
        if (response.detectedSourceLanguage.isEmpty())
            response.detectedSourceLanguage = item.value(QStringLiteral("detectedSourceLanguage")).toString();
    }
    response.ok = true;
    return response;
}

} // namespace hypers
