#pragma once

#include <QColor>
#include <QIcon>

class QApplication;

namespace hypers::theme {

// Palette of the dark/red HYPERS look. The stylesheet in
// resources/styles/dark.qss uses the same values.
inline QColor background() { return QColor(0x0E, 0x0E, 0x11); }
inline QColor surface() { return QColor(0x16, 0x16, 0x1B); }
inline QColor surfaceRaised() { return QColor(0x1E, 0x1E, 0x25); }
inline QColor border() { return QColor(0x2A, 0x2A, 0x33); }
inline QColor text() { return QColor(0xEC, 0xEC, 0xF1); }
inline QColor textMuted() { return QColor(0x9A, 0x9A, 0xA8); }
inline QColor accent() { return QColor(0xE5, 0x26, 0x2E); }
inline QColor accentHover() { return QColor(0xFF, 0x3B, 0x43); }
inline QColor accentPressed() { return QColor(0xB8, 0x1D, 0x24); }
inline QColor success() { return QColor(0x3D, 0xD6, 0x8C); }

void apply(QApplication& app);

QIcon appIcon();
QIcon trayIcon(bool paused);

} // namespace hypers::theme
