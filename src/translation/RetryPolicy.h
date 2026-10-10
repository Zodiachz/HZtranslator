#pragma once

#include <QByteArray>

#include <optional>

namespace hypers {

// Exponential backoff with jitter. Retry n (1-based) waits
// min(maxDelay, base * 2^(n-1)) +/- jitter, but never less than a server
// supplied Retry-After (capped at kMaxRetryAfterMs).
struct RetryPolicy {
    static constexpr int kMaxRetryAfterMs = 30000;

    int maxRetries = 3;
    int baseDelayMs = 500;
    int maxDelayMs = 8000;
    double jitterRatio = 0.2;

    // `random01` in [0, 1) - injectable for deterministic tests.
    int delayMs(int retryNumber, std::optional<int> retryAfterMs, double random01) const;
    int delayMs(int retryNumber, std::optional<int> retryAfterMs = std::nullopt) const;

    // Parses an HTTP Retry-After header (delta-seconds or HTTP-date) into milliseconds.
    static std::optional<int> parseRetryAfter(const QByteArray& headerValue);
};

} // namespace hypers
