#include "translation/TranslationTypes.h"

namespace hypers {

bool TranslationError::isRetryable() const
{
    switch (kind) {
    case TranslationErrorKind::Network:
    case TranslationErrorKind::Timeout:
    case TranslationErrorKind::RateLimited:
    case TranslationErrorKind::ServerError:
        return true;
    default:
        return false;
    }
}

QString TranslationError::userMessage() const
{
    switch (kind) {
    case TranslationErrorKind::None:
        return {};
    case TranslationErrorKind::NotConfigured:
        return QStringLiteral("No API key is set for the selected provider. Add one in Settings › Translation.");
    case TranslationErrorKind::UnsupportedLanguage:
        return QStringLiteral("The selected provider does not support this language. "
                              "Pick another target language or switch provider.");
    case TranslationErrorKind::InvalidInput:
        return message.isEmpty() ? QStringLiteral("Nothing to translate.") : message;
    case TranslationErrorKind::Network:
        return QStringLiteral("Network error - check your internet connection or proxy. (%1)").arg(message);
    case TranslationErrorKind::Timeout:
        return QStringLiteral("The translation service did not respond in time.");
    case TranslationErrorKind::Authentication:
        return QStringLiteral("The API key was rejected. Verify it in Settings › Translation. (%1)").arg(message);
    case TranslationErrorKind::QuotaExceeded:
        return QStringLiteral("Your translation quota is used up for this billing period.");
    case TranslationErrorKind::RateLimited:
        return QStringLiteral("Too many requests - the provider is rate limiting. Try again in a moment.");
    case TranslationErrorKind::BadRequest:
        return QStringLiteral("The provider rejected the request: %1").arg(message);
    case TranslationErrorKind::ServerError:
        return QStringLiteral("The translation service is having problems (HTTP %1). Try again later.")
            .arg(httpStatus);
    case TranslationErrorKind::InvalidResponse:
        return QStringLiteral("Unexpected response from the translation service.");
    case TranslationErrorKind::Canceled:
        return QStringLiteral("Translation canceled.");
    }
    return message;
}

QString errorKindName(TranslationErrorKind kind)
{
    switch (kind) {
    case TranslationErrorKind::None: return QStringLiteral("None");
    case TranslationErrorKind::NotConfigured: return QStringLiteral("NotConfigured");
    case TranslationErrorKind::UnsupportedLanguage: return QStringLiteral("UnsupportedLanguage");
    case TranslationErrorKind::InvalidInput: return QStringLiteral("InvalidInput");
    case TranslationErrorKind::Network: return QStringLiteral("Network");
    case TranslationErrorKind::Timeout: return QStringLiteral("Timeout");
    case TranslationErrorKind::Authentication: return QStringLiteral("Authentication");
    case TranslationErrorKind::QuotaExceeded: return QStringLiteral("QuotaExceeded");
    case TranslationErrorKind::RateLimited: return QStringLiteral("RateLimited");
    case TranslationErrorKind::BadRequest: return QStringLiteral("BadRequest");
    case TranslationErrorKind::ServerError: return QStringLiteral("ServerError");
    case TranslationErrorKind::InvalidResponse: return QStringLiteral("InvalidResponse");
    case TranslationErrorKind::Canceled: return QStringLiteral("Canceled");
    }
    return QStringLiteral("Unknown");
}

TranslationError errorForHttpStatus(int httpStatus, const QString& message)
{
    TranslationError error;
    error.httpStatus = httpStatus;
    error.message = message.isEmpty() ? QStringLiteral("HTTP %1").arg(httpStatus) : message;
    if (httpStatus == 401 || httpStatus == 403)
        error.kind = TranslationErrorKind::Authentication;
    else if (httpStatus == 408)
        error.kind = TranslationErrorKind::Timeout;
    else if (httpStatus == 413 || httpStatus == 414)
        error.kind = TranslationErrorKind::InvalidInput;
    else if (httpStatus == 429)
        error.kind = TranslationErrorKind::RateLimited;
    else if (httpStatus >= 500)
        error.kind = TranslationErrorKind::ServerError;
    else if (httpStatus >= 400)
        error.kind = TranslationErrorKind::BadRequest;
    else
        error.kind = TranslationErrorKind::InvalidResponse;
    if (error.kind == TranslationErrorKind::InvalidInput && message.isEmpty())
        error.message = QStringLiteral("The text is too long for a single request.");
    return error;
}

} // namespace hypers
