#pragma once

#include "translation/TranslationTypes.h"

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>

namespace hypers {

// A translation backend described as a pure request/response adapter.
// Providers never touch the network themselves: TranslationService owns the
// QNetworkAccessManager, timeouts, retries, batching and caching. This keeps
// providers small and fully unit-testable, and adding a backend (Azure,
// LibreTranslate, ...) means implementing just this interface.
class ITranslationProvider {
public:
    virtual ~ITranslationProvider() = default;

    virtual QString id() const = 0;          // stable, persisted: "deepl", "google"
    virtual QString displayName() const = 0; // "DeepL"

    virtual void setApiKey(const QString& apiKey) = 0;
    virtual bool hasApiKey() const = 0;

    // Request limits used by the service to batch segments.
    virtual int maxSegmentsPerRequest() const = 0;
    virtual int maxCharactersPerRequest() const = 0;

    // App language codes (see Languages).
    virtual bool supportsSourceLanguage(const QString& code) const = 0;
    virtual bool supportsTargetLanguage(const QString& code) const = 0;

    virtual QNetworkRequest buildRequest() const = 0;
    virtual QByteArray buildPayload(const QStringList& segments,
                                    const QString& sourceLanguage,
                                    const QString& targetLanguage) const = 0;

    // httpStatus is 200 for success; body is the raw response body.
    virtual ProviderResponse parseResponse(int httpStatus, const QByteArray& body) const = 0;
};

} // namespace hypers
