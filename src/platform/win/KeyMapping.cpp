#include "platform/win/KeyMapping.h"

#include <windows.h>

namespace hypers {

unsigned int qtKeyToVirtualKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return 'A' + static_cast<unsigned int>(key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return '0' + static_cast<unsigned int>(key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + static_cast<unsigned int>(key - Qt::Key_F1);

    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Menu: return VK_APPS;
    case Qt::Key_MediaPlay:
    case Qt::Key_MediaPause:
    case Qt::Key_MediaTogglePlayPause: return VK_MEDIA_PLAY_PAUSE;
    case Qt::Key_MediaNext: return VK_MEDIA_NEXT_TRACK;
    case Qt::Key_MediaPrevious: return VK_MEDIA_PREV_TRACK;
    case Qt::Key_VolumeUp: return VK_VOLUME_UP;
    case Qt::Key_VolumeDown: return VK_VOLUME_DOWN;
    case Qt::Key_VolumeMute: return VK_VOLUME_MUTE;
    default: break;
    }

    // Printable characters (punctuation, layout specific keys): ask the active
    // keyboard layout which virtual key produces the character.
    if (key >= 0x20 && key < 0x10000) {
        const SHORT scan = ::VkKeyScanW(static_cast<wchar_t>(key));
        if (scan != -1)
            return static_cast<unsigned int>(LOBYTE(scan));
    }
    return 0;
}

std::optional<NativeHotkey> toNativeHotkey(const QKeySequence& sequence, QString* error)
{
    auto fail = [error](const QString& message) -> std::optional<NativeHotkey> {
        if (error)
            *error = message;
        return std::nullopt;
    };

    if (sequence.isEmpty())
        return fail(QStringLiteral("No shortcut set."));
    if (sequence.count() > 1)
        return fail(QStringLiteral("Global shortcuts must be a single key combination (no chords)."));

    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    const int key = combination.key();

    if (key == 0 || key == Qt::Key_unknown || key == Qt::Key_Control || key == Qt::Key_Shift
        || key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_AltGr) {
        return fail(QStringLiteral("Add a non-modifier key to the shortcut."));
    }

    NativeHotkey hotkey;
    if (mods & Qt::ControlModifier)
        hotkey.modifiers |= MOD_CONTROL;
    if (mods & Qt::AltModifier)
        hotkey.modifiers |= MOD_ALT;
    if (mods & Qt::ShiftModifier)
        hotkey.modifiers |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)
        hotkey.modifiers |= MOD_WIN;

    const bool isFunctionKey = key >= Qt::Key_F1 && key <= Qt::Key_F24;
    const bool isMediaKey = key == Qt::Key_MediaPlay || key == Qt::Key_MediaPause
        || key == Qt::Key_MediaTogglePlayPause || key == Qt::Key_MediaNext || key == Qt::Key_MediaPrevious
        || key == Qt::Key_VolumeUp || key == Qt::Key_VolumeDown || key == Qt::Key_VolumeMute;
    const bool isStandaloneOk = isFunctionKey || isMediaKey || key == Qt::Key_Pause || key == Qt::Key_Print
        || key == Qt::Key_ScrollLock;
    if (hotkey.modifiers == 0 && !isStandaloneOk)
        return fail(QStringLiteral("Use at least one modifier (Ctrl, Alt, Shift or Win) so normal typing isn't blocked."));
    if (hotkey.modifiers == MOD_SHIFT && !isFunctionKey)
        return fail(QStringLiteral("Shift alone would block typing capital letters/symbols - add Ctrl or Alt."));

    hotkey.virtualKey = qtKeyToVirtualKey(key);
    if (hotkey.virtualKey == 0)
        return fail(QStringLiteral("This key can't be used as a global shortcut on the current keyboard layout."));
    return hotkey;
}

} // namespace hypers
