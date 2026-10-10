#include "translation/DeepLProvider.h"

#include "core/Languages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace hypers {

bool DeepLProvider::supportsSourceLanguage(const QString& code) const
{
    if (code == QLatin1String(Languages::kAuto))
        return true;
    const LanguageInfo* lang = Languages::find(code);
    return lang && !lang->deeplSource.isEmpty();
}

bool DeepLProvider::supportsTargetLanguage(const QString& code) const
{
    const LanguageInfo* lang = Languages::find(code);
    return lang && !lang->deeplTarget.isEmpty();
}

QNetworkRequest DeepLProvider::buildRequest() const
{
    const QString host = isFreeKey() ? QStringLiteral("https://api-free.deepl.com")
                                     : QStringLiteral("https://api.deepl.com");
    QNetworkRequest request(QUrl(host + QStringLiteral("/v2/translate")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "DeepL-Auth-Key " + m_apiKey.toUtf8());
    request.setRawHeader("Accept", "application/json");
    return request;
}

QByteArray DeepLProvider::buildPayload(const QStringList& segments,
                                       const QString& sourceLanguage,
                                       const QString& targetLanguage) const
{
    QJsonObject body;
    body.insert(QStringLiteral("text"), QJsonArray::fromStringList(segments));
    if (const LanguageInfo* target = Languages::find(targetLanguage))
        body.insert(QStringLiteral("target_lang"), target->deeplTarget);
    if (sourceLanguage != QLatin1String(Languages::kAuto)) {
        if (const LanguageInfo* source = Languages::find(sourceLanguage))
            body.insert(QStringLiteral("source_lang"), source->deeplSource);
    }
    // Keep the user's punctuation/casing - important for chat messages.
    body.insert(QStringLiteral("preserve_formatting"), true);
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

ProviderResponse DeepLProvider::parseResponse(int httpStatus, const QByteArray& body) const
{
    ProviderResponse response;
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (httpStatus != 200) {
        QString message;
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            message = obj.value(QStringLiteral("message")).toString();
            const QString detail = obj.value(QStringLiteral("detail")).toString();
            if (!detail.isEmpty())
                message += (message.isEmpty() ? QString() : QStringLiteral(": ")) + detail;
        }
        response.error = errorForHttpStatus(httpStatus, message);
        if (httpStatus == 456)
            response.error.kind = TranslationErrorKind::QuotaExceeded;
        else if (httpStatus == 529)
            response.error.kind = TranslationErrorKind::RateLimited; // "too many requests" (high load)
        else if (httpStatus == 400 && message.contains(QLatin1String("target_lang"), Qt::CaseInsensitive))
            response.error.kind = TranslationErrorKind::UnsupportedLanguage;
        return response;
    }

    if (!doc.isObject()) {
        response.error.kind = TranslationErrorKind::InvalidResponse;
        response.error.message = parseError.errorString();
        response.error.httpStatus = httpStatus;
        return response;
    }

    const QJsonArray translations = doc.object().value(QStringLiteral("translations")).toArray();
    if (translations.isEmpty()) {
        response.error.kind = TranslationErrorKind::InvalidResponse;
        response.error.message = QStringLiteral("Missing 'translations' array");
        response.error.httpStatus = httpStatus;
        return response;
    }
    for (const QJsonValue& value : translations) {
        const QJsonObject item = value.toObject();
        response.translations.append(item.value(QStringLiteral("text")).toString());
        if (response.detectedSourceLanguage.isEmpty())
            response.detectedSourceLanguage = item.value(QStringLiteral("detected_source_language")).toString();
    }
    response.ok = true;
    return response;
}

} // namespace hypers
