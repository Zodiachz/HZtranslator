#pragma once

#include <QKeySequence>
#include <QLineEdit>

namespace hypers {

// Records a single global shortcut. Unlike QKeySequenceEdit it accepts exactly
// one combination, uses the physical key (so Ctrl+Alt+T is recorded as "T"
// even on an Arabic or Russian keyboard layout) and announces when it is
// recording, so global hotkeys can be released meanwhile.
class HotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget* parent = nullptr);

    QKeySequence keySequence() const { return m_sequence; }
    void setKeySequence(const QKeySequence& sequence);

Q_SIGNALS:
    void recordingChanged(bool recording);
    void keySequenceEdited(const QKeySequence& sequence);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void setRecording(bool recording);
    void refreshText(Qt::KeyboardModifiers pendingModifiers = Qt::NoModifier);

    QKeySequence m_sequence;
    bool m_recording = false;
};

} // namespace hypers
