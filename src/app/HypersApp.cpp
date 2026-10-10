#include "app/HypersApp.h"

#include "core/HistoryStore.h"
#include "features/OcrTranslator.h"
#include "features/SelectionTranslator.h"
#include "platform/ISecretStore.h"
#include "platform/win/AutoStart.h"
#include "platform/win/GlobalHotkeyManager.h"
#include "platform/win/WinCredentialStore.h"
#include "translation/DeepLProvider.h"
#include "translation/GoogleTranslateProvider.h"
#include "translation/TranslationService.h"
#include "ui/MainWindow.h"
#include "ui/ResultPopup.h"
#include "ui/SettingsPage.h"
#include "ui/TranslatePage.h"
#include "ui/TrayController.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcApp, "hypers.app")

namespace hypers {

HypersApp::HypersApp(QObject* parent)
    : QObject(parent)
{
}

HypersApp::~HypersApp()
{
    // Features first: they may still touch the popup/tray while cancelling work.
    delete m_selection;
    m_selection = nullptr;
    delete m_ocr;
    m_ocr = nullptr;
    m_window.reset();
    m_popup.reset();
}

bool HypersApp::initialize(bool launchedAtLogin)
{
    m_settings = std::make_unique<AppSettings>();
    m_secrets = std::make_unique<WinCredentialStore>();

    // Translation layer
    m_translation = new TranslationService(this);
    m_translation->registerProvider(std::make_unique<DeepLProvider>());
    m_translation->registerProvider(std::make_unique<GoogleTranslateProvider>());
    loadApiKeys();
    applyTranslationSettings();

    // History
    m_history = new HistoryStore(this);
    QString historyError;
    if (!m_history->open(HistoryStore::defaultPath(), &historyError))
        qCWarning(lcApp) << "History unavailable:" << historyError;
    m_history->setMaxEntries(m_settings->historyLimit());

    // Platform + features
    m_hotkeys = new GlobalHotkeyManager(this);
    m_popup = std::make_unique<ResultPopup>();
    m_popup->setAutoHideSeconds(m_settings->popupTimeoutSeconds());
    m_selection = new SelectionTranslator(m_settings.get(), m_translation, m_history, m_popup.get(), this);
    m_ocr = new OcrTranslator(m_settings.get(), m_translation, m_history, m_popup.get(), this);

    // UI
    m_context.settings = m_settings.get();
    m_context.secrets = m_secrets.get();
    m_context.translation = m_translation;
    m_context.history = m_history;
    m_context.hotkeys = m_hotkeys;
    m_tray = new TrayController(this);
    m_window = std::make_unique<MainWindow>(m_context);

    // Wiring
    connect(m_hotkeys, &GlobalHotkeyManager::triggered, this, &HypersApp::onHotkey);
    connect(m_hotkeys, &GlobalHotkeyManager::pausedChanged, this, [this](bool paused) {
        m_tray->setPaused(paused);
        m_window->setHotkeysPaused(paused);
    });
    connect(m_selection, &SelectionTranslator::notification, this, &HypersApp::notify);
    connect(m_ocr, &OcrTranslator::notification, this, &HypersApp::notify);
    connect(m_popup.get(), &ResultPopup::openInAppRequested, this, [this](const QString& text) {
        m_window->translatePage()->setSourceText(text, true);
        m_window->showPage(MainWindow::Page::Translate);
        showMainWindow();
    });

    connect(m_tray, &TrayController::openRequested, this, &HypersApp::showMainWindow);
    connect(m_tray, &TrayController::ocrRequested, this, [this] {
        // Let the tray menu finish fading out so it isn't part of the screenshot.
        QTimer::singleShot(250, m_ocr, &OcrTranslator::trigger);
    });
    connect(m_tray, &TrayController::historyRequested, this, [this] {
        m_window->showPage(MainWindow::Page::History);
        showMainWindow();
    });
    connect(m_tray, &TrayController::settingsRequested, this, [this] {
        m_window->showPage(MainWindow::Page::Settings);
        showMainWindow();
    });
    connect(m_tray, &TrayController::pauseToggled, m_hotkeys, &GlobalHotkeyManager::setPaused);
    connect(m_tray, &TrayController::quitRequested, this, &HypersApp::quit);

    connect(m_window.get(), &MainWindow::hiddenToTray, this, [this] {
        if (!m_settings->trayHintShown()) {
            notify(QStringLiteral("HYPERS TRANSLATOR is still running"),
                   QStringLiteral("Your shortcuts keep working from the tray. Right-click the tray icon to quit."),
                   false);
            m_settings->setTrayHintShown(true);
        }
    });
    connect(m_window.get(), &MainWindow::quitRequested, this, &HypersApp::quit);
    connect(m_settings.get(), &AppSettings::changed, this, &HypersApp::onSettingChanged);

    m_tray->show();
    registerHotkeysFromSettings();
    updateHotkeyHints();
    autostart::refreshPathIfEnabled();

    const ITranslationProvider* provider = m_translation->activeProvider();
    if (!provider || !provider->hasApiKey()) {
        // First run (or key removed): take the user straight to the key field.
        m_window->showPage(MainWindow::Page::Settings);
        showMainWindow();
        m_window->settingsPage()->focusApiKey();
        notify(QStringLiteral("Welcome to HYPERS TRANSLATOR"),
               QStringLiteral("Add a DeepL or Google Cloud API key to start translating."), false);
    } else if (!(launchedAtLogin && m_settings->startMinimized())) {
        showMainWindow();
    }
    qCInfo(lcApp) << "Ready - provider" << m_translation->activeProviderId();
    return true;
}

void HypersApp::loadApiKeys()
{
    for (ITranslationProvider* provider : m_translation->providers()) {
        QString error;
        const std::optional<QString> key = m_secrets->read(apiKeySecretName(provider->id()), &error);
        if (key)
            provider->setApiKey(*key);
        else if (!error.isEmpty())
            qCWarning(lcApp) << "Could not read API key for" << provider->id() << ":" << error;
    }
}

void HypersApp::applyTranslationSettings()
{
    m_translation->setActiveProvider(m_settings->providerId());
    m_translation->setRequestTimeoutMs(m_settings->requestTimeoutMs());
    RetryPolicy policy = m_translation->retryPolicy();
    policy.maxRetries = m_settings->maxRetries();
    m_translation->setRetryPolicy(policy);
}

void HypersApp::registerHotkeysFromSettings()
{
    QStringList failures;
    for (HotkeyAction action :
         {HotkeyAction::TranslateSelection, HotkeyAction::OcrRegion, HotkeyAction::ShowWindow}) {
        const QKeySequence sequence = m_settings->hotkey(action);
        if (sequence.isEmpty())
            continue;
        QString error;
        if (!m_hotkeys->setHotkey(action, sequence, &error))
            failures << error;
    }
    if (!failures.isEmpty())
        notify(QStringLiteral("Shortcut unavailable"),
               failures.join(QLatin1Char('\n')) + QStringLiteral("\nChange it in Settings › Global shortcuts."), true);
}

void HypersApp::onHotkey(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::TranslateSelection:
        m_selection->trigger(m_hotkeys->nativeHotkey(action));
        break;
    case HotkeyAction::OcrRegion:
        m_ocr->trigger();
        break;
    case HotkeyAction::ShowWindow:
        showMainWindow();
        break;
    }
}

void HypersApp::onSettingChanged(const QString& key)
{
    if (key.startsWith(QLatin1String("translation/provider")) || key.startsWith(QLatin1String("network/")))
        applyTranslationSettings();
    else if (key == QLatin1String("behavior/popupTimeoutSec"))
        m_popup->setAutoHideSeconds(m_settings->popupTimeoutSeconds());
    else if (key == QLatin1String("history/maxEntries"))
        m_history->setMaxEntries(m_settings->historyLimit());
    else if (key.startsWith(QLatin1String("hotkeys/")))
        updateHotkeyHints();
}

void HypersApp::updateHotkeyHints()
{
    m_tray->setHotkeyHints(m_settings->hotkey(HotkeyAction::TranslateSelection).toString(QKeySequence::NativeText),
                           m_settings->hotkey(HotkeyAction::OcrRegion).toString(QKeySequence::NativeText));
}

void HypersApp::notify(const QString& title, const QString& message, bool isError)
{
    if (isError)
        qCWarning(lcApp) << "Notification:" << title << "-" << message;
    m_tray->notify(title, message, isError);
}

void HypersApp::showMainWindow()
{
    m_window->present();
}

void HypersApp::quit()
{
    qCInfo(lcApp) << "Quitting";
    m_popup->hide();
    m_window->prepareForQuit();
    m_window->close();
    QCoreApplication::quit();
}

} // namespace hypers
