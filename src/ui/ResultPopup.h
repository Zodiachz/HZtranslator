#pragma once

#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QWidget>

class QFrame;
class QLabel;
class QPropertyAnimation;
class QPushButton;
class QScrollArea;
class QToolButton;

namespace hypers {

// Frameless, always-on-top card that shows progress, results and errors next
// to where the user is working. It is shown without activation so the app
// underneath keeps keyboard focus (important for the replace workflow).
class ResultPopup : public QWidget {
    Q_OBJECT
public:
    explicit ResultPopup(QWidget* parent = nullptr);

    void setAutoHideSeconds(int seconds); // 0 = never

    // `anchor` is a global (logical) rectangle the popup should sit next to;
    // pass a null QRect to keep the current position.
    void showLoading(const QString& status, const QRect& anchor);
    void updateStatus(const QString& status);
    void showResult(const QString& title, const QString& original, const QString& translated, const QRect& anchor);
    void showError(const QString& message, const QRect& anchor, const QString& original = QString());

    bool isPinned() const { return m_pinned; }

Q_SIGNALS:
    void openInAppRequested(const QString& original);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class Mode { Loading, Result, Error };

    void setMode(Mode mode);
    void presentAt(const QRect& anchor);
    void placeNear(const QRect& anchor);
    void restartAutoHide();
    void updateTextHeights();
    void setOriginalVisible(bool visible);

    Mode m_mode = Mode::Loading;
    QFrame* m_card = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_pin = nullptr;
    QToolButton* m_close = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_text = nullptr;
    QScrollArea* m_textScroll = nullptr;
    QLabel* m_original = nullptr;
    QScrollArea* m_originalScroll = nullptr;
    QLabel* m_error = nullptr;
    QWidget* m_actions = nullptr;
    QPushButton* m_toggleOriginal = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_open = nullptr;
    QPropertyAnimation* m_fade = nullptr;

    QTimer m_autoHide;
    QTimer m_dots;
    int m_dotCount = 0;
    QString m_statusBase;
    QString m_originalText;
    QString m_translatedText;
    int m_autoHideSeconds = 15;
    bool m_pinned = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
};

} // namespace hypers
