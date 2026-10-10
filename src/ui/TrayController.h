#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

namespace hypers {

class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject* parent = nullptr);
    ~TrayController() override;

    void show();
    void setPaused(bool paused);
    void setHotkeyHints(const QString& selectionHotkey, const QString& ocrHotkey);
    void notify(const QString& title, const QString& message, bool isError);

Q_SIGNALS:
    void openRequested();
    void ocrRequested();
    void historyRequested();
    void settingsRequested();
    void pauseToggled(bool paused);
    void quitRequested();

private:
    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
    QAction* m_ocrAction;
    QAction* m_pauseAction;
    bool m_paused = false;
    QString m_selectionHint;
    QString m_ocrHint;
};

} // namespace hypers
