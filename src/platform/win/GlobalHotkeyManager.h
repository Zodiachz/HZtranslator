#pragma once

#include "core/AppSettings.h"
#include "platform/win/KeyMapping.h"

#include <QHash>
#include <QKeySequence>
#include <QObject>

#include <memory>

namespace hypers {

class MessageWindow;

// System-wide hotkeys via RegisterHotKey(). WM_HOTKEY is delivered to a
// message-only window owned by this object and re-emitted as triggered().
class GlobalHotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkeyManager(QObject* parent = nullptr);
    ~GlobalHotkeyManager() override;

    // Registers (or replaces) the hotkey for `action`. An empty sequence just
    // unregisters it. On failure the previous hotkey stays active.
    bool setHotkey(HotkeyAction action, const QKeySequence& sequence, QString* error = nullptr);
    QKeySequence hotkey(HotkeyAction action) const;
    std::optional<NativeHotkey> nativeHotkey(HotkeyAction action) const;

    // Temporarily releases all hotkeys, e.g. while the user records a new
    // shortcut (RegisterHotKey would otherwise swallow the key press).
    void setSuspended(bool suspended);
    bool isSuspended() const { return m_suspendCount > 0; }

    // User-facing pause from the tray menu.
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

Q_SIGNALS:
    void triggered(hypers::HotkeyAction action);
    void pausedChanged(bool paused);

private:
    struct Entry {
        QKeySequence sequence;
        NativeHotkey native;
        bool registered = false;
    };

    bool registerEntry(int id, Entry& entry, QString* error);
    void unregisterEntry(int id, Entry& entry);
    void applyActiveState();
    bool isActive() const { return m_suspendCount == 0 && !m_paused; }

    std::unique_ptr<MessageWindow> m_window;
    QHash<int, Entry> m_entries;
    int m_suspendCount = 0;
    bool m_paused = false;
};

} // namespace hypers
