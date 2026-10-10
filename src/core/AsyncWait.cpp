#include "core/AsyncWait.h"

#include <QElapsedTimer>
#include <QTimer>

#include <memory>

namespace hypers {

void waitUntil(QObject* context,
               std::function<bool()> condition,
               int timeoutMs,
               std::function<void(bool)> done,
               int intervalMs)
{
    if (condition()) {
        done(true);
        return;
    }

    // The timer is parented to `context`, so it dies (and stops) with it.
    auto* timer = new QTimer(context);
    timer->setInterval(intervalMs);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();

    QObject::connect(timer, &QTimer::timeout, timer,
                     [timer, elapsed, timeoutMs,
                      condition = std::move(condition), done = std::move(done)] {
                         const bool satisfied = condition();
                         if (!satisfied && elapsed->elapsed() < timeoutMs)
                             return;
                         timer->stop();
                         timer->deleteLater();
                         done(satisfied);
                     });
    timer->start();
}

} // namespace hypers
