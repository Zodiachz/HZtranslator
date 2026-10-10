#pragma once

#include <functional>

class QObject;

namespace hypers {

// Polls `condition` on the GUI thread every `intervalMs` without blocking the
// event loop. Calls `done(true)` as soon as the condition holds, or
// `done(false)` after `timeoutMs`. Nothing is called if `context` is destroyed
// first. The condition is evaluated once immediately.
void waitUntil(QObject* context,
               std::function<bool()> condition,
               int timeoutMs,
               std::function<void(bool satisfied)> done,
               int intervalMs = 10);

} // namespace hypers
