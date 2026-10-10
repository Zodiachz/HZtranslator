#pragma once

#include <QByteArray>
#include <QKeySequence>
#include <QObject>
#include <QSettings>
#include <QString>

namespace hypers {

enum class SelectionAction {
    Replace, // paste the translation over the selection
    Popup,   // show the translation in a floating popup
    Copy     // put the translation on the clipboard
};

enum class HotkeyAction {
    TranslateSelection = 1,
    OcrRegion = 2,
    ShowWindow = 3,
};

// Typed facade over QSettings (HKCU\Software\Hypers\HYPERS TRANSLATOR).
// API keys are intentionally NOT stored here - see ISecretStore.
class AppSettings : public QObject {
    Q_OBJECT
public:
    explicit AppSettings(QObject* parent = nullptr);

    // Translation
    QString providerId() const;
    void setProviderId(const QString& id);
    QString targetLanguage() const;
    void setTargetLanguage(const QString& code);
    QString sourceLanguage() const; // "auto" or a language code
    void setSourceLanguage(const QString& code);

    // Hotkeys
    QKeySequence hotkey(HotkeyAction action) const;
    void setHotkey(HotkeyAction action, const QKeySequence& sequence);
    static QKeySequence defaultHotkey(HotkeyAction action);

    // Behaviour
    SelectionAction selectionAction() const;
    void setSelectionAction(SelectionAction action);
    int popupTimeoutSeconds() const; // 0 = stay until closed
    void setPopupTimeoutSeconds(int seconds);
    int clipboardRestoreDelayMs() const;
    void setClipboardRestoreDelayMs(int ms);
    int copyTimeoutMs() const;
    void setCopyTimeoutMs(int ms);

    // OCR
    QString ocrLanguage() const; // empty = Windows user profile languages
    void setOcrLanguage(const QString& tag);
    bool ocrMergeLines() const;
    void setOcrMergeLines(bool merge);

    // Network
    int requestTimeoutMs() const;
    void setRequestTimeoutMs(int ms);
    int maxRetries() const;
    void setMaxRetries(int retries);

    // System
    // True once autostart was set up on first run; afterwards only the user changes it.
    bool autostartConfigured() const;
    void setAutostartConfigured(bool configured);
    bool closeToTray() const;
    void setCloseToTray(bool toTray);
    bool trayHintShown() const;
    void setTrayHintShown(bool shown);

    // History
    bool historyEnabled() const;
    void setHistoryEnabled(bool enabled);
    int historyLimit() const;
    void setHistoryLimit(int limit);

    // Window state
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    static QString hotkeyKey(HotkeyAction action);

Q_SIGNALS:
    // Emitted after any setter changed a value. `key` is the QSettings key.
    void changed(const QString& key);

private:
    void write(const QString& key, const QVariant& value);

    mutable QSettings m_settings;
};

} // namespace hypers
