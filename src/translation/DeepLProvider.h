#pragma once

#include "translation/ITranslationProvider.h"

namespace hypers {

// DeepL API v2 (https://developers.deepl.com/docs/api-reference/translate).
// Free-plan keys end with ":fx" and use api-free.deepl.com; Pro keys use api.deepl.com.
class DeepLProvider final : public ITranslationProvider {
public:
    QString id() const override { return QStringLiteral("deepl"); }
    QString displayName() const override { return QStringLiteral("DeepL"); }

    void setApiKey(const QString& apiKey) override { m_apiKey = apiKey.trimmed(); }
    bool hasApiKey() const override { return !m_apiKey.isEmpty(); }

    int maxSegmentsPerRequest() const override { return 50; }
    int maxCharactersPerRequest() const override { return 30000; } // request body limit is 128 KiB

    bool supportsSourceLanguage(const QString& code) const override;
    bool supportsTargetLanguage(const QString& code) const override;

    QNetworkRequest buildRequest() const override;
    QByteArray buildPayload(const QStringList& segments,
                            const QString& sourceLanguage,
                            const QString& targetLanguage) const override;
    ProviderResponse parseResponse(int httpStatus, const QByteArray& body) const override;

    bool isFreeKey() const { return m_apiKey.endsWith(QLatin1String(":fx")); }

private:
    QString m_apiKey;
};

} // namespace hypers
