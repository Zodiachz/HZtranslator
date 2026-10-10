#include "platform/win/GlobalHotkeyManager.h"

#include "platform/win/MessageWindow.h"

#include <QLoggingCategory>
#include <QMetaObject>

#include <windows.h>

Q_LOGGING_CATEGORY(lcHotkey, "hypers.hotkey")

namespace hypers {

namespace {

QString registrationError(DWORD code, const QKeySequence& sequence)
{
    const QString shortcut = sequence.toString(QKeySequence::NativeText);
    if (code == ERROR_HOTKEY_ALREADY_REGISTERED) {
        return QStringLiteral("%1 is already taken by Windows or another application. Choose a different shortcut.")
            .arg(shortcut);
    }
    return QStringLiteral("Windows refused to register %1 (error %2).").arg(shortcut).arg(code);
}

} // namespace

GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent)
    : QObject(parent)
{
    m_window = std::make_unique<MessageWindow>([this](UINT message, WPARAM wParam, LPARAM, LRESULT* result) {
        if (message != WM_HOTKEY)
            return false;
        const int id = static_cast<int>(wParam);
        // Leave the window procedure before running feature code (which may
        // open windows or spin nested event loops).
        QMetaObject::invokeMethod(
            this, [this, id] { Q_EMIT triggered(static_cast<HotkeyAction>(id)); }, Qt::QueuedConnection);
        *result = 0;
        return true;
    });
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
        unregisterEntry(it.key(), it.value());
}

bool GlobalHotkeyManager::registerEntry(int id, Entry& entry, QString* error)
{
    if (entry.registered || entry.sequence.isEmpty())
        return true;
    if (!m_window || !m_window->hwnd()) {
        if (error)
            *error = QStringLiteral("Hotkey window is not available.");
        return false;
    }
    // MOD_NOREPEAT: holding the keys down fires once, not on every auto-repeat.
    if (!::RegisterHotKey(m_window->hwnd(), id, entry.native.modifiers | MOD_NOREPEAT, entry.native.virtualKey)) {
        const DWORD err = ::GetLastError();
        if (error)
            *error = registrationError(err, entry.sequence);
        qCWarning(lcHotkey) << "RegisterHotKey failed for" << entry.sequence << "error" << err;
        return false;
    }
    entry.registered = true;
    return true;
}

void GlobalHotkeyManager::unregisterEntry(int id, Entry& entry)
{
    if (!entry.registered)
        return;
    if (m_window && m_window->hwnd())
        ::UnregisterHotKey(m_window->hwnd(), id);
    entry.registered = false;
}

bool GlobalHotkeyManager::setHotkey(HotkeyAction action, const QKeySequence& sequence, QString* error)
{
    const int id = static_cast<int>(action);

    if (sequence.isEmpty()) {
        auto it = m_entries.find(id);
        if (it != m_entries.end()) {
            unregisterEntry(id, it.value());
            m_entries.erase(it);
        }
        return true;
    }

    const std::optional<NativeHotkey> native = toNativeHotkey(sequence, error);
    if (!native)
        return false;

    // Reject duplicates inside the app before asking Windows.
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.key() != id && it.value().native == *native) {
            if (error)
                *error = QStringLiteral("%1 is already used by another HYPERS TRANSLATOR action.")
                             .arg(sequence.toString(QKeySequence::NativeText));
            return false;
        }
    }

    Entry candidate;
    candidate.sequence = sequence;
    candidate.native = *native;

    Entry previous;
    const bool hadPrevious = m_entries.contains(id);
    if (hadPrevious) {
        previous = m_entries.value(id);
        if (previous.native == candidate.native) {
            m_entries[id].sequence = sequence;
            return true;
        }
        unregisterEntry(id, m_entries[id]);
        previous.registered = false;
    }

    if (isActive() && !registerEntry(id, candidate, error)) {
        if (hadPrevious) {
            QString ignored;
            registerEntry(id, previous, &ignored);
            m_entries[id] = previous;
        }
        return false;
    }
    m_entries[id] = candidate;
    qCInfo(lcHotkey) << "Hotkey" << id << "=" << sequence.toString(QKeySequence::PortableText);
    return true;
}

QKeySequence GlobalHotkeyManager::hotkey(HotkeyAction action) const
{
    return m_entries.value(static_cast<int>(action)).sequence;
}

std::optional<NativeHotkey> GlobalHotkeyManager::nativeHotkey(HotkeyAction action) const
{
    const auto it = m_entries.constFind(static_cast<int>(action));
    if (it == m_entries.cend())
        return std::nullopt;
    return it.value().native;
}

void GlobalHotkeyManager::applyActiveState()
{
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (isActive()) {
            QString error;
            if (!registerEntry(it.key(), it.value(), &error))
                qCWarning(lcHotkey) << "Could not re-register hotkey:" << error;
        } else {
            unregisterEntry(it.key(), it.value());
        }
    }
}

void GlobalHotkeyManager::setSuspended(bool suspended)
{
    m_suspendCount = qMax(0, m_suspendCount + (suspended ? 1 : -1));
    applyActiveState();
}

void GlobalHotkeyManager::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    applyActiveState();
    Q_EMIT pausedChanged(paused);
}

} // namespace hypers
