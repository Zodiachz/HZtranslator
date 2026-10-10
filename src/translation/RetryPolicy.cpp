#include "translation/RetryPolicy.h"

#include <QDateTime>
#include <QLocale>
#include <QRandomGenerator>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace hypers {

int RetryPolicy::delayMs(int retryNumber, std::optional<int> retryAfterMs, double random01) const
{
    const int exponent = std::clamp(retryNumber - 1, 0, 20);
    const double raw = std::min<double>(maxDelayMs, baseDelayMs * std::pow(2.0, exponent));
    const double jitter = raw * jitterRatio * (random01 * 2.0 - 1.0); // +/- jitterRatio
    int delay = static_cast<int>(std::lround(std::max(0.0, raw + jitter)));
    if (retryAfterMs)
        delay = std::max(delay, std::min(*retryAfterMs, kMaxRetryAfterMs));
    return delay;
}

int RetryPolicy::delayMs(int retryNumber, std::optional<int> retryAfterMs) const
{
    return delayMs(retryNumber, retryAfterMs, QRandomGenerator::global()->generateDouble());
}

std::optional<int> RetryPolicy::parseRetryAfter(const QByteArray& headerValue)
{
    const QByteArray value = headerValue.trimmed();
    if (value.isEmpty())
        return std::nullopt;

    bool ok = false;
    const int seconds = value.toInt(&ok);
    if (ok)
        return seconds < 0 ? std::nullopt : std::optional<int>(std::min(seconds, 3600) * 1000);

    // RFC 7231 IMF-fixdate, e.g. "Wed, 21 Oct 2015 07:28:00 GMT"
    QDateTime when = QLocale::c().toDateTime(QString::fromLatin1(value), QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"));
    if (!when.isValid())
        return std::nullopt;
    when.setTimeZone(QTimeZone::utc());
    const qint64 ms = QDateTime::currentDateTimeUtc().msecsTo(when);
    return static_cast<int>(std::clamp<qint64>(ms, 0, 3600 * 1000));
}

} // namespace hypers
