#pragma once

namespace hypers {

class AppSettings;
class GlobalHotkeyManager;
class HistoryStore;
class ISecretStore;
class TranslationService;

// Long-lived services shared by the UI. Owned by HypersApp.
struct AppContext {
    AppSettings* settings = nullptr;
    ISecretStore* secrets = nullptr;
    TranslationService* translation = nullptr;
    HistoryStore* history = nullptr;
    GlobalHotkeyManager* hotkeys = nullptr;
};

} // namespace hypers
