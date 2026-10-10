#pragma once

#include "app/AppContext.h"

#include <QList>
#include <QMainWindow>

class QLabel;
class QPushButton;
class QStackedWidget;

namespace hypers {

class HistoryPage;
class SettingsPage;
class TranslatePage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    enum class Page { Translate = 0, History = 1, Settings = 2, About = 3 };

    explicit MainWindow(const AppContext& context, QWidget* parent = nullptr);

    void showPage(Page page);
    void present(); // show, restore and focus
    TranslatePage* translatePage() const { return m_translatePage; }
    SettingsPage* settingsPage() const { return m_settingsPage; }

    void setHotkeysPaused(bool paused);
    // Lets the next close event through (application exit) instead of hiding to tray.
    void prepareForQuit() { m_quitting = true; }

Q_SIGNALS:
    void hiddenToTray();
    void quitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildSidebar();
    QWidget* buildAboutPage();
    void updateStatusPill();

    AppContext m_ctx;
    QStackedWidget* m_stack = nullptr;
    QList<QPushButton*> m_navButtons;
    TranslatePage* m_translatePage = nullptr;
    HistoryPage* m_historyPage = nullptr;
    SettingsPage* m_settingsPage = nullptr;
    QLabel* m_statusPill = nullptr;
    QLabel* m_providerLabel = nullptr;
    bool m_paused = false;
    bool m_frameStyled = false;
    bool m_quitting = false;
};

} // namespace hypers
