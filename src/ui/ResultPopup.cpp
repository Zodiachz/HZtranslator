#include "ui/ResultPopup.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QEnterEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace hypers {

namespace {

constexpr int kShadowMargin = 18;  // transparent border that hosts the drop shadow
constexpr int kCardWidth = 460;
constexpr int kMaxTextHeight = 320;
constexpr int kMaxOriginalHeight = 140;

QLabel* makeTextLabel(const char* objectName)
{
    auto* label = new QLabel;
    label->setObjectName(QLatin1String(objectName));
    label->setTextFormat(Qt::PlainText); // translations may contain '<' - never interpret as HTML
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setCursor(Qt::IBeamCursor);
    return label;
}

QScrollArea* wrapInScroll(QLabel* label)
{
    auto* scroll = new QScrollArea;
    scroll->setWidget(label);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

void applyDirection(QLabel* label, const QString& text)
{
    const bool rtl = text.isRightToLeft();
    label->setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    // AlignLeft means the *leading* edge, i.e. the right side in a right-to-left label.
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

} // namespace

ResultPopup::ResultPopup(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle(QStringLiteral("HYPERS TRANSLATOR"));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadowMargin, kShadowMargin, kShadowMargin, kShadowMargin);

    m_card = new QFrame;
    m_card->setObjectName(QStringLiteral("popupCard"));
    m_card->setFixedWidth(kCardWidth);
    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 170));
    m_card->setGraphicsEffect(shadow);
    outer->addWidget(m_card);

    auto* layout = new QVBoxLayout(m_card);
    layout->setContentsMargins(16, 12, 12, 14);
    layout->setSpacing(10);

    // Header: title + pin + close
    auto* header = new QHBoxLayout;
    header->setSpacing(4);
    m_title = new QLabel(QStringLiteral("HYPERS TRANSLATOR"));
    m_title->setObjectName(QStringLiteral("popupTitle"));
    header->addWidget(m_title, 1);

    m_pin = new QToolButton;
    m_pin->setObjectName(QStringLiteral("popupIconButton"));
    m_pin->setCheckable(true);
    m_pin->setText(QStringLiteral("Pin"));
    m_pin->setToolTip(QStringLiteral("Keep this popup open"));
    m_pin->setCursor(Qt::PointingHandCursor);
    header->addWidget(m_pin);

    m_close = new QToolButton;
    m_close->setObjectName(QStringLiteral("popupIconButton"));
    m_close->setText(QStringLiteral("✕"));
    m_close->setToolTip(QStringLiteral("Close (Esc)"));
    m_close->setCursor(Qt::PointingHandCursor);
    header->addWidget(m_close);
    layout->addLayout(header);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("popupStatus"));
    layout->addWidget(m_status);

    m_text = makeTextLabel("popupText");
    m_textScroll = wrapInScroll(m_text);
    layout->addWidget(m_textScroll);

    m_error = makeTextLabel("popupError");
    layout->addWidget(m_error);

    m_original = makeTextLabel("popupOriginal");
    m_originalScroll = wrapInScroll(m_original);
    layout->addWidget(m_originalScroll);

    m_actions = new QWidget;
    auto* actions = new QHBoxLayout(m_actions);
    actions->setContentsMargins(0, 2, 0, 0);
    actions->setSpacing(8);
    m_toggleOriginal = new QPushButton(QStringLiteral("Show original"));
    m_toggleOriginal->setProperty("ghost", true);
    m_toggleOriginal->setCursor(Qt::PointingHandCursor);
    actions->addWidget(m_toggleOriginal);
    actions->addStretch(1);
    m_open = new QPushButton(QStringLiteral("Open in app"));
    m_open->setCursor(Qt::PointingHandCursor);
    actions->addWidget(m_open);
    m_copy = new QPushButton(QStringLiteral("Copy"));
    m_copy->setProperty("accent", true);
    m_copy->setCursor(Qt::PointingHandCursor);
    actions->addWidget(m_copy);
    layout->addWidget(m_actions);

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(140);
    m_fade->setStartValue(0.0);
    m_fade->setEndValue(1.0);

    m_autoHide.setSingleShot(true);
    connect(&m_autoHide, &QTimer::timeout, this, &QWidget::hide);

    m_dots.setInterval(350);
    connect(&m_dots, &QTimer::timeout, this, [this] {
        m_dotCount = (m_dotCount + 1) % 4;
        m_status->setText(m_statusBase + QString(m_dotCount, QLatin1Char('.')));
    });

    connect(m_close, &QToolButton::clicked, this, &QWidget::hide);
    connect(m_pin, &QToolButton::toggled, this, [this](bool pinned) {
        m_pinned = pinned;
        if (pinned)
            m_autoHide.stop();
        else
            restartAutoHide();
    });
    connect(m_toggleOriginal, &QPushButton::clicked, this,
            [this] { setOriginalVisible(!m_originalScroll->isVisible()); });
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_translatedText);
        m_copy->setText(QStringLiteral("Copied ✓"));
        QTimer::singleShot(1500, m_copy, [this] { m_copy->setText(QStringLiteral("Copy")); });
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        Q_EMIT openInAppRequested(m_originalText);
        hide();
    });

    setMode(Mode::Loading);
}

void ResultPopup::setAutoHideSeconds(int seconds)
{
    m_autoHideSeconds = qMax(0, seconds);
}

void ResultPopup::setMode(Mode mode)
{
    m_mode = mode;
    const bool loading = mode == Mode::Loading;
    const bool result = mode == Mode::Result;
    const bool error = mode == Mode::Error;

    m_status->setVisible(loading);
    m_textScroll->setVisible(result);
    m_error->setVisible(error);
    m_actions->setVisible(result || (error && !m_originalText.isEmpty()));
    m_copy->setVisible(result);
    m_open->setVisible(result || error);
    m_toggleOriginal->setVisible(!m_originalText.isEmpty() && result);
    if (!result)
        m_originalScroll->setVisible(error && !m_originalText.isEmpty());

    if (loading) {
        m_dotCount = 0;
        m_dots.start();
        m_autoHide.stop();
    } else {
        m_dots.stop();
    }
}

void ResultPopup::showLoading(const QString& status, const QRect& anchor)
{
    m_originalText.clear();
    m_translatedText.clear();
    m_statusBase = status;
    m_status->setText(status);
    m_title->setText(QStringLiteral("HYPERS TRANSLATOR"));
    setMode(Mode::Loading);
    presentAt(anchor);
}

void ResultPopup::updateStatus(const QString& status)
{
    m_statusBase = status;
    m_status->setText(status);
}

void ResultPopup::showResult(const QString& title, const QString& original, const QString& translated,
                             const QRect& anchor)
{
    m_originalText = original;
    m_translatedText = translated;
    m_title->setText(title.toUpper());
    m_text->setText(translated);
    applyDirection(m_text, translated);
    m_original->setText(original);
    applyDirection(m_original, original);
    m_copy->setText(QStringLiteral("Copy"));
    setMode(Mode::Result);
    setOriginalVisible(false);
    presentAt(anchor);
    restartAutoHide();
}

void ResultPopup::showError(const QString& message, const QRect& anchor, const QString& original)
{
    m_originalText = original;
    m_translatedText.clear();
    m_title->setText(QStringLiteral("HYPERS TRANSLATOR"));
    m_error->setText(message);
    m_original->setText(original);
    applyDirection(m_original, original);
    setMode(Mode::Error);
    presentAt(anchor);
    restartAutoHide();
}

void ResultPopup::setOriginalVisible(bool visible)
{
    m_originalScroll->setVisible(visible && !m_originalText.isEmpty());
    m_toggleOriginal->setText(visible ? QStringLiteral("Hide original") : QStringLiteral("Show original"));
    updateTextHeights();
    adjustSize();
}

void ResultPopup::updateTextHeights()
{
    const int contentWidth = kCardWidth - 28 - 12; // card margins + scrollbar allowance
    const int textHeight = m_text->heightForWidth(contentWidth);
    m_textScroll->setFixedHeight(qBound(24, textHeight + 4, kMaxTextHeight));
    const int originalHeight = m_original->heightForWidth(contentWidth);
    m_originalScroll->setFixedHeight(qBound(20, originalHeight + 4, kMaxOriginalHeight));
}

void ResultPopup::presentAt(const QRect& anchor)
{
    updateTextHeights();
    m_card->adjustSize();
    adjustSize();
    if (!anchor.isNull())
        placeNear(anchor);
    if (!isVisible()) {
        setWindowOpacity(0.0);
        show();
        m_fade->start();
    }
    raise();
}

void ResultPopup::placeNear(const QRect& anchor)
{
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen)
        screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    const QSize size = sizeHint().expandedTo(this->size());

    constexpr int gap = 6;
    int x = anchor.left() - kShadowMargin;
    int y = anchor.bottom() + gap - kShadowMargin + 1;
    if (y + size.height() - kShadowMargin > available.bottom()) {
        // Not enough room below: go above the anchor.
        y = anchor.top() - gap - size.height() + kShadowMargin;
    }
    if (y + kShadowMargin < available.top()) {
        // Neither fits (huge anchor such as a full-screen selection): overlay at the anchor's top.
        y = qMax(available.top(), anchor.top()) - kShadowMargin;
    }
    x = qBound(available.left() - kShadowMargin, x, available.right() - size.width() + kShadowMargin);
    y = qBound(available.top() - kShadowMargin, y, available.bottom() - size.height() + kShadowMargin);
    move(x, y);
}

void ResultPopup::restartAutoHide()
{
    if (m_pinned || m_autoHideSeconds <= 0 || m_mode == Mode::Loading || underMouse())
        return;
    m_autoHide.start(m_autoHideSeconds * 1000);
}

void ResultPopup::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (event->matches(QKeySequence::Copy) && m_mode == Mode::Result && !m_text->hasSelectedText()) {
        QGuiApplication::clipboard()->setText(m_translatedText);
        return;
    }
    QWidget::keyPressEvent(event);
}

void ResultPopup::enterEvent(QEnterEvent* event)
{
    m_autoHide.stop();
    QWidget::enterEvent(event);
}

void ResultPopup::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (m_mode != Mode::Loading && !m_pinned && m_autoHideSeconds > 0)
        m_autoHide.start(m_autoHideSeconds * 1000);
}

void ResultPopup::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ResultPopup::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ResultPopup::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

} // namespace hypers
