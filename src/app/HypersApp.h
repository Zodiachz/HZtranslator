#pragma once

#include "app/AppContext.h"
#include "core/AppSettings.h"

#include <QObject>

#include <memory>

namespace hypers {

class GlobalHotkeyManager;
class HistoryStore;
class MainWindow;
class OcrTranslator;
class ResultPopup;
class SelectionTranslator;
class TranslationService;
class TrayController;
class WinCredentialStore;

// Composition root: creates the services, wires features, UI and tray together.
class HypersApp : public QObject {
    Q_OBJECT
public:
    explicit HypersApp(QObject* parent = nullptr);
    ~HypersApp() override;

    // `launchedAtLogin` = started through the Windows Run key.
    bool initialize(bool launchedAtLogin);

public Q_SLOTS:
    void showMainWindow();
    void quit();

private:
    void loadApiKeys();
    void applyTranslationSettings();
    void registerHotkeysFromSettings();
    void onHotkey(HotkeyAction action);
    void onSettingChanged(const QString& key);
    void updateHotkeyHints();
    void notify(const QString& title, const QString& message, bool isError);

    std::unique_ptr<AppSettings> m_settings;
    std::unique_ptr<WinCredentialStore> m_secrets;
    TranslationService* m_translation = nullptr;
    HistoryStore* m_history = nullptr;
    GlobalHotkeyManager* m_hotkeys = nullptr;
    std::unique_ptr<ResultPopup> m_popup;
    SelectionTranslator* m_selection = nullptr;
    OcrTranslator* m_ocr = nullptr;
    TrayController* m_tray = nullptr;
    std::unique_ptr<MainWindow> m_window;
    AppContext m_context;
};

} // namespace hypers
