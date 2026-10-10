#pragma once

#include "translation/ITranslationProvider.h"

namespace hypers {

// Google Cloud Translation - Basic (v2) REST API with an API key
// (https://cloud.google.com/translate/docs/reference/rest/v2/translate).
// The key is sent in the X-Goog-Api-Key header so it never appears in URLs or proxy logs.
class GoogleTranslateProvider final : public ITranslationProvider {
public:
    QString id() const override { return QStringLiteral("google"); }
    QString displayName() const override { return QStringLiteral("Google Cloud Translate"); }

    void setApiKey(const QString& apiKey) override { m_apiKey = apiKey.trimmed(); }
    bool hasApiKey() const override { return !m_apiKey.isEmpty(); }

    int maxSegmentsPerRequest() const override { return 128; }
    int maxCharactersPerRequest() const override { return 5000; } // Google's recommended maximum

    bool supportsSourceLanguage(const QString& code) const override;
    bool supportsTargetLanguage(const QString& code) const override;

    QNetworkRequest buildRequest() const override;
    QByteArray buildPayload(const QStringList& segments,
                            const QString& sourceLanguage,
                            const QString& targetLanguage) const override;
    ProviderResponse parseResponse(int httpStatus, const QByteArray& body) const override;

private:
    QString m_apiKey;
};

} // namespace hypers
