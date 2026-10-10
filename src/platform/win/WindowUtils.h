#pragma once

#include <QColor>
#include <QPoint>
#include <QRect>
#include <QString>

#include <optional>

#include <windows.h>

class QWidget;

namespace hypers::winutil {

HWND foregroundWindow();

// Executable file name of the process owning `hwnd`, e.g. "Discord.exe".
QString processNameForWindow(HWND hwnd);

bool isOwnProcessWindow(HWND hwnd);

// True if the process owning `hwnd` runs at a higher integrity level than we
// do (typically "Run as administrator"). Windows UIPI then silently drops our
// synthetic keystrokes, so Ctrl+C / Ctrl+V can't be sent to it.
bool isWindowMoreElevatedThanUs(HWND hwnd);

bool isCurrentProcessElevated();

// Caret rectangle (global, physical pixels) of the given window's GUI thread,
// if the application exposes a system caret. Many modern apps (Chromium,
// Electron, WinUI) don't - callers fall back to the mouse position.
std::optional<QRect> caretRectForWindow(HWND hwnd);

// Converts a physical-pixel point (Win32) to Qt's device-independent global coordinates.
QPoint physicalToLogical(const QPoint& physical);

// Windows 11 frame styling: dark title bar, caption/border colours, rounded corners.
void applyDarkWindowFrame(QWidget* window, const QColor& caption, const QColor& border);

// Rounded corners + native shadow for frameless popups (Windows 11; no-op on 10).
void applyRoundedCorners(QWidget* window, bool smallRadius = false);

// Gives keyboard focus to one of our windows. Works right after a hotkey
// because the process that receives WM_HOTKEY is allowed to take foreground.
void bringToForeground(QWidget* window);

} // namespace hypers::winutil
