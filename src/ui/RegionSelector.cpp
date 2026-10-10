#include "ui/RegionSelector.h"

#include "platform/win/WindowUtils.h"
#include "ui/Theme.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QWidget>
#include <QWindow>

#include <functional>

Q_LOGGING_CATEGORY(lcRegion, "hypers.region")

namespace hypers {

class SelectionOverlay : public QWidget {
public:
    using SelectedFn = std::function<void(SelectionOverlay*, const QRect&)>;
    using CanceledFn = std::function<void()>;

    SelectionOverlay(QScreen* screen, QImage shot, SelectedFn onSelected, CanceledFn onCanceled)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::NoDropShadowWindowHint)
        , m_shot(std::move(shot))
        , m_onSelected(std::move(onSelected))
        , m_onCanceled(std::move(onCanceled))
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::CrossCursor);
        setWindowTitle(QStringLiteral("HYPERS TRANSLATOR - select region"));

        // Bind the native window to the right monitor before sizing it, so Qt
        // uses that monitor's DPI.
        winId();
        if (QWindow* window = windowHandle())
            window->setScreen(screen);
        setGeometry(screen->geometry());

        const int logicalWidth = qMax(1, screen->geometry().width());
        m_dpr = static_cast<qreal>(m_shot.width()) / logicalWidth;
        m_cursor = mapFromGlobal(QCursor::pos());
    }

    // `logical` is in widget coordinates.
    QImage crop(const QRect& logical) const { return m_shot.copy(toPhysical(logical)); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.drawImage(QRectF(rect()), m_shot, QRectF(m_shot.rect()));

        const QRect selection = currentSelection();
        QPainterPath dim;
        dim.addRect(QRectF(rect()));
        if (!selection.isEmpty()) {
            QPainterPath hole;
            hole.addRect(QRectF(selection));
            dim = dim.subtracted(hole);
        }
        p.fillPath(dim, QColor(8, 8, 12, 150));

        const QColor accent = theme::accent();
        if (!selection.isEmpty()) {
            p.setPen(QPen(accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(selection).adjusted(0.5, 0.5, -0.5, -0.5));

            // Corner handles
            p.setBrush(accent);
            p.setPen(Qt::NoPen);
            constexpr int h = 6;
            for (const QPoint& c : {selection.topLeft(), selection.topRight(), selection.bottomLeft(),
                                    selection.bottomRight()}) {
                p.drawRect(QRect(c.x() - h / 2, c.y() - h / 2, h, h));
            }

            // Size badge (physical pixels - what OCR will actually get)
            const QRect physical = toPhysical(selection);
            const QString label = QStringLiteral("%1 × %2").arg(physical.width()).arg(physical.height());
            QFont font = p.font();
            font.setPointSizeF(9);
            font.setBold(true);
            p.setFont(font);
            const QRect textRect = p.fontMetrics().boundingRect(label).adjusted(-8, -4, 8, 4);
            QPoint badgePos = selection.topLeft() + QPoint(0, -textRect.height() - 6);
            if (badgePos.y() < 0)
                badgePos = selection.topLeft() + QPoint(6, 6);
            const QRect badge(badgePos, textRect.size());
            p.setBrush(accent);
            p.drawRoundedRect(badge, 5, 5);
            p.setPen(Qt::white);
            p.drawText(badge, Qt::AlignCenter, label);
        } else if (rect().contains(m_cursor)) {
            // Crosshair guides while hovering
            p.setPen(QPen(QColor(229, 38, 46, 140), 1, Qt::DashLine));
            p.drawLine(0, m_cursor.y(), width(), m_cursor.y());
            p.drawLine(m_cursor.x(), 0, m_cursor.x(), height());
        }

        // Instruction banner
        const QString hint = QStringLiteral("Drag to select the text to translate   ·   Esc or right-click to cancel");
        QFont font = p.font();
        font.setPointSizeF(10.5);
        font.setBold(true);
        p.setFont(font);
        const QRect hintRect = p.fontMetrics().boundingRect(hint).adjusted(-18, -10, 18, 10);
        const QRect banner(QPoint((width() - hintRect.width()) / 2, 28), hintRect.size());
        if (!banner.intersects(selection)) {
            p.setPen(QPen(QColor(255, 255, 255, 30), 1));
            p.setBrush(QColor(20, 20, 26, 225));
            p.drawRoundedRect(banner, 10, 10);
            p.setPen(theme::text());
            p.drawText(banner, Qt::AlignCenter, hint);
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::RightButton) {
            m_onCanceled();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            m_selecting = true;
            m_origin = event->position().toPoint();
            m_cursor = m_origin;
            update();
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        m_cursor = event->position().toPoint();
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || !m_selecting)
            return;
        m_cursor = event->position().toPoint();
        const QRect selection = currentSelection();
        m_selecting = false;
        if (selection.width() >= 6 && selection.height() >= 6) {
            m_onSelected(this, selection);
            return;
        }
        update(); // a click without dragging: keep waiting
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Escape) {
            m_onCanceled();
            return;
        }
        QWidget::keyPressEvent(event);
    }

private:
    QRect currentSelection() const
    {
        if (!m_selecting)
            return {};
        return QRect(m_origin, m_cursor).normalized().intersected(rect());
    }

    QRect toPhysical(const QRect& logical) const
    {
        const QRect physical(qRound(logical.x() * m_dpr), qRound(logical.y() * m_dpr),
                             qRound(logical.width() * m_dpr), qRound(logical.height() * m_dpr));
        return physical.intersected(m_shot.rect());
    }

    QImage m_shot;
    SelectedFn m_onSelected;
    CanceledFn m_onCanceled;
    qreal m_dpr = 1.0;
    bool m_selecting = false;
    QPoint m_origin;
    QPoint m_cursor;
};

RegionSelector::RegionSelector(QObject* parent)
    : QObject(parent)
{
}

RegionSelector::~RegionSelector()
{
    closeOverlays();
}

bool RegionSelector::isActive() const
{
    for (const QPointer<SelectionOverlay>& overlay : m_overlays) {
        if (overlay)
            return true;
    }
    return false;
}

void RegionSelector::start()
{
    if (isActive())
        return;

    // Grab every screen first so no overlay ends up in another screen's capture.
    QList<QPair<QScreen*, QImage>> shots;
    for (QScreen* screen : QGuiApplication::screens()) {
        QImage image = screen->grabWindow(0).toImage();
        if (image.isNull()) {
            qCWarning(lcRegion) << "Could not capture screen" << screen->name();
            continue;
        }
        shots.append({screen, image});
    }
    if (shots.isEmpty()) {
        Q_EMIT canceled();
        return;
    }

    QScreen* cursorScreen = QGuiApplication::screenAt(QCursor::pos());
    SelectionOverlay* focusOverlay = nullptr;
    for (const auto& [screen, image] : shots) {
        auto* overlay = new SelectionOverlay(
            screen, image,
            [this](SelectionOverlay* source, const QRect& rect) { onOverlaySelected(source, rect); },
            [this] {
                closeOverlays();
                Q_EMIT canceled();
            });
        m_overlays.append(overlay);
        overlay->show();
        if (screen == cursorScreen || !focusOverlay)
            focusOverlay = overlay;
    }
    winutil::bringToForeground(focusOverlay);
    focusOverlay->setFocus(Qt::ActiveWindowFocusReason);
}

void RegionSelector::cancel()
{
    if (!isActive())
        return;
    closeOverlays();
    Q_EMIT canceled();
}

void RegionSelector::onOverlaySelected(SelectionOverlay* overlay, const QRect& localRect)
{
    const QImage image = overlay->crop(localRect);
    const QRect globalRect = localRect.translated(overlay->geometry().topLeft());
    closeOverlays();
    Q_EMIT regionSelected(image, globalRect);
}

void RegionSelector::closeOverlays()
{
    const QList<QPointer<SelectionOverlay>> overlays = std::exchange(m_overlays, {});
    for (const QPointer<SelectionOverlay>& overlay : overlays) {
        if (overlay)
            overlay->close(); // WA_DeleteOnClose
    }
}

} // namespace hypers
