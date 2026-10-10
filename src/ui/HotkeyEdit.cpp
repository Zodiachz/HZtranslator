#include "ui/HotkeyEdit.h"

#include <QKeyEvent>
#include <QStyle>

namespace hypers {

namespace {

bool isModifierKey(int key)
{
    return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr || key == Qt::Key_Super_L || key == Qt::Key_Super_R;
}

QString modifierText(Qt::KeyboardModifiers mods)
{
    QString text;
    if (mods & Qt::ControlModifier)
        text += QStringLiteral("Ctrl+");
    if (mods & Qt::AltModifier)
        text += QStringLiteral("Alt+");
    if (mods & Qt::ShiftModifier)
        text += QStringLiteral("Shift+");
    if (mods & Qt::MetaModifier)
        text += QStringLiteral("Win+");
    return text;
}

// Prefer the physical key for letters/digits so shortcuts are layout independent.
int physicalKey(const QKeyEvent* event)
{
    const quint32 vk = event->nativeVirtualKey();
    if (vk >= 'A' && vk <= 'Z')
        return Qt::Key_A + static_cast<int>(vk - 'A');
    if (vk >= '0' && vk <= '9')
        return Qt::Key_0 + static_cast<int>(vk - '0');
    return event->key();
}

} // namespace

HotkeyEdit::HotkeyEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setReadOnly(true);
    setFocusPolicy(Qt::ClickFocus);
    setContextMenuPolicy(Qt::NoContextMenu);
    setPlaceholderText(QStringLiteral("Not set"));
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Click, then press the new shortcut.\nEsc cancels · Backspace clears"));
    refreshText();
}

void HotkeyEdit::setKeySequence(const QKeySequence& sequence)
{
    m_sequence = sequence;
    refreshText();
}

void HotkeyEdit::refreshText(Qt::KeyboardModifiers pendingModifiers)
{
    if (m_recording) {
        const QString mods = modifierText(pendingModifiers);
        setText(mods.isEmpty() ? QStringLiteral("Press a shortcut…") : mods + QStringLiteral("…"));
        return;
    }
    setText(m_sequence.toString(QKeySequence::NativeText));
}

void HotkeyEdit::setRecording(bool recording)
{
    if (m_recording == recording)
        return;
    m_recording = recording;
    setProperty("recording", recording);
    style()->unpolish(this);
    style()->polish(this);
    refreshText();
    Q_EMIT recordingChanged(recording);
}

bool HotkeyEdit::event(QEvent* event)
{
    if (m_recording) {
        // Swallow Tab and application shortcuts while recording.
        if (event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            keyPressEvent(static_cast<QKeyEvent*>(event));
            return true;
        }
    }
    return QLineEdit::event(event);
}

void HotkeyEdit::keyPressEvent(QKeyEvent* event)
{
    if (!m_recording) {
        QLineEdit::keyPressEvent(event);
        return;
    }
    event->accept();
    const int key = event->key();
    const Qt::KeyboardModifiers mods =
        event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);

    if (isModifierKey(key) || key == Qt::Key_unknown || key == 0) {
        refreshText(mods);
        return;
    }
    if (mods == Qt::NoModifier && key == Qt::Key_Escape) {
        clearFocus(); // cancel
        return;
    }
    if (mods == Qt::NoModifier && (key == Qt::Key_Backspace || key == Qt::Key_Delete)) {
        m_sequence = QKeySequence();
        clearFocus();
        Q_EMIT keySequenceEdited(m_sequence);
        return;
    }

    m_sequence = QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(physicalKey(event))));
    clearFocus();
    Q_EMIT keySequenceEdited(m_sequence);
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent* event)
{
    if (m_recording) {
        const Qt::KeyboardModifiers mods =
            event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
        refreshText(mods);
        event->accept();
        return;
    }
    QLineEdit::keyReleaseEvent(event);
}

void HotkeyEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    setRecording(true);
}

void HotkeyEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);
    setRecording(false);
}

void HotkeyEdit::mousePressEvent(QMouseEvent* event)
{
    QLineEdit::mousePressEvent(event);
    setFocus(Qt::MouseFocusReason);
}

} // namespace hypers
