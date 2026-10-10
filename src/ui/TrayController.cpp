#include "ui/TrayController.h"

#include "ui/Theme.h"

#include <QAction>
#include <QMenu>
#include <QTimer>

#include <memory>

namespace hypers {

TrayController::TrayController(QObject* parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(theme::trayIcon(false), this))
    , m_menu(new QMenu)
{
    QAction* open = m_menu->addAction(QStringLiteral("Open HYPERS TRANSLATOR"));
    QFont bold = open->font();
    bold.setBold(true);
    open->setFont(bold);
    connect(open, &QAction::triggered, this, &TrayController::openRequested);

    m_ocrAction = m_menu->addAction(QStringLiteral("Translate screen region"));
    connect(m_ocrAction, &QAction::triggered, this, &TrayController::ocrRequested);

    m_menu->addSeparator();
    QAction* history = m_menu->addAction(QStringLiteral("History"));
    connect(history, &QAction::triggered, this, &TrayController::historyRequested);
    QAction* settings = m_menu->addAction(QStringLiteral("Settings"));
    connect(settings, &QAction::triggered, this, &TrayController::settingsRequested);

    m_menu->addSeparator();
    m_pauseAction = m_menu->addAction(QStringLiteral("Pause global hotkeys"));
    m_pauseAction->setCheckable(true);
    connect(m_pauseAction, &QAction::toggled, this, &TrayController::pauseToggled);

    m_menu->addSeparator();
    QAction* quit = m_menu->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, this, &TrayController::quitRequested);

    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(QStringLiteral("HYPERS TRANSLATOR"));
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            Q_EMIT openRequested();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &TrayController::openRequested);
}

TrayController::~TrayController()
{
    m_tray->hide();
    delete m_menu; // QMenu has no QObject parent (QSystemTrayIcon doesn't take ownership)
}

void TrayController::show()
{
    m_tray->show();
    if (QSystemTrayIcon::isSystemTrayAvailable())
        return;

    // At sign-in the app can start before Explorer's taskbar exists. Keep
    // checking and re-add the icon once the notification area is ready.
    auto* retry = new QTimer(this);
    retry->setInterval(2000);
    auto attempts = std::make_shared<int>(0);
    connect(retry, &QTimer::timeout, this, [this, retry, attempts] {
        if (QSystemTrayIcon::isSystemTrayAvailable() || ++*attempts >= 90) {
            m_tray->hide();
            m_tray->show();
            retry->stop();
            retry->deleteLater();
        }
    });
    retry->start();
}

void TrayController::setPaused(bool paused)
{
    m_paused = paused;
    if (m_pauseAction->isChecked() != paused)
        m_pauseAction->setChecked(paused);
    m_tray->setIcon(theme::trayIcon(paused));
    setHotkeyHints(m_selectionHint, m_ocrHint);
}

void TrayController::setHotkeyHints(const QString& selectionHotkey, const QString& ocrHotkey)
{
    m_selectionHint = selectionHotkey;
    m_ocrHint = ocrHotkey;
    m_ocrAction->setText(ocrHotkey.isEmpty() ? QStringLiteral("Translate screen region")
                                             : QStringLiteral("Translate screen region\t%1").arg(ocrHotkey));
    QString tip = QStringLiteral("HYPERS TRANSLATOR");
    if (m_paused) {
        tip += QStringLiteral("\nHotkeys paused");
    } else {
        if (!selectionHotkey.isEmpty())
            tip += QStringLiteral("\nSelection: %1").arg(selectionHotkey);
        if (!ocrHotkey.isEmpty())
            tip += QStringLiteral("\nScreen OCR: %1").arg(ocrHotkey);
    }
    m_tray->setToolTip(tip);
}

void TrayController::notify(const QString& title, const QString& message, bool isError)
{
    m_tray->showMessage(title, message, isError ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information,
                        isError ? 8000 : 4000);
}

} // namespace hypers
