#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

#include <optional>

namespace hypers {

struct TranslationRequest {
    QString text;
    QString sourceLanguage = QStringLiteral("auto"); // app language code or "auto"
    QString targetLanguage;                          // app language code
};

struct TranslationResult {
    QString translatedText;
    QString detectedSourceLanguage; // provider code as reported, e.g. "EN" / "en"
    QString providerId;
    bool fromCache = false;
};

enum class TranslationErrorKind {
    None,
    NotConfigured,       // no API key
    UnsupportedLanguage, // provider can't translate to/from the requested language
    InvalidInput,        // empty text, too long, ...
    Network,             // DNS, connection reset, TLS, ...
    Timeout,
    Authentication,      // bad / revoked key, API not enabled
    QuotaExceeded,       // monthly character quota used up
    RateLimited,         // too many requests right now
    BadRequest,
    ServerError,
    InvalidResponse,     // unparsable / unexpected JSON
    Canceled,
};

struct TranslationError {
    TranslationErrorKind kind = TranslationErrorKind::None;
    QString message;                   // technical message (provider/Qt), safe to log
    int httpStatus = 0;
    std::optional<int> retryAfterMs;   // from Retry-After, if any

    bool isRetryable() const;
    // Short, actionable text for end users.
    QString userMessage() const;
};

// What a provider extracts from a single HTTP response.
struct ProviderResponse {
    bool ok = false;
    QStringList translations;      // same order as the request segments
    QString detectedSourceLanguage;
    TranslationError error;
};

QString errorKindName(TranslationErrorKind kind);

// Generic HTTP status -> error mapping shared by providers; providers refine it
// with their own status codes / error reasons.
TranslationError errorForHttpStatus(int httpStatus, const QString& message);

} // namespace hypers

Q_DECLARE_METATYPE(hypers::TranslationResult)
Q_DECLARE_METATYPE(hypers::TranslationError)
