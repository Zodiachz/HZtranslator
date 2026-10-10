#pragma once

#include <QString>

namespace hypers::autostart {

// "Start with Windows" through HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// (per-user, no admin rights needed). Also honours the Task Manager
// "Startup apps" toggle, stored under Explorer\StartupApproved\Run.

constexpr const char* kLaunchArgument = "--autostart";

bool isEnabled();
bool setEnabled(bool enabled, QString* error = nullptr);

// If autostart is on but the executable moved (portable install), point the
// Run entry at the current path.
void refreshPathIfEnabled();

} // namespace hypers::autostart
