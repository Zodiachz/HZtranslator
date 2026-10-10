#include "core/AppSettings.h"

#include "core/Languages.h"

#include <QtGlobal>

namespace hypers {

namespace {
namespace keys {
const QString Provider = QStringLiteral("translation/provider");
const QString Target = QStringLiteral("translation/targetLanguage");
const QString Source = QStringLiteral("translation/sourceLanguage");
const QString SelectionActionKey = QStringLiteral("behavior/selectionAction");
const QString PopupTimeout = QStringLiteral("behavior/popupTimeoutSec");
const QString RestoreDelay = QStringLiteral("behavior/clipboardRestoreDelayMs");
const QString CopyTimeout = QStringLiteral("behavior/copyTimeoutMs");
const QString OcrLanguage = QStringLiteral("ocr/language");
const QString OcrMerge = QStringLiteral("ocr/mergeLines");
const QString RequestTimeout = QStringLiteral("network/timeoutMs");
const QString MaxRetries = QStringLiteral("network/maxRetries");
const QString AutostartConfigured = QStringLiteral("system/autostartConfigured");
const QString CloseToTray = QStringLiteral("system/closeToTray");
const QString TrayHint = QStringLiteral("system/trayHintShown");
const QString HistoryEnabled = QStringLiteral("history/enabled");
const QString HistoryLimit = QStringLiteral("history/maxEntries");
const QString Geometry = QStringLiteral("ui/mainWindowGeometry");
} // namespace keys
} // namespace

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
{
}

void AppSettings::write(const QString& key, const QVariant& value)
{
    if (m_settings.contains(key) && m_settings.value(key) == value)
        return;
    m_settings.setValue(key, value);
    Q_EMIT changed(key);
}

QString AppSettings::providerId() const
{
    const QString id = m_settings.value(keys::Provider, QStringLiteral("deepl")).toString();
    return (id == QLatin1String("google")) ? id : QStringLiteral("deepl");
}

void AppSettings::setProviderId(const QString& id) { write(keys::Provider, id); }

QString AppSettings::targetLanguage() const
{
    const QString code = m_settings.value(keys::Target, QStringLiteral("en-US")).toString();
    return Languages::find(code) ? code : QStringLiteral("en-US");
}

void AppSettings::setTargetLanguage(const QString& code) { write(keys::Target, code); }

QString AppSettings::sourceLanguage() const
{
    const QString code = m_settings.value(keys::Source, QString::fromLatin1(Languages::kAuto)).toString();
    return (code == QLatin1String(Languages::kAuto) || Languages::find(code))
        ? code
        : QString::fromLatin1(Languages::kAuto);
}

void AppSettings::setSourceLanguage(const QString& code) { write(keys::Source, code); }

QString AppSettings::hotkeyKey(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::TranslateSelection: return QStringLiteral("hotkeys/translateSelection");
    case HotkeyAction::OcrRegion: return QStringLiteral("hotkeys/ocrRegion");
    case HotkeyAction::ShowWindow: return QStringLiteral("hotkeys/showWindow");
    }
    return {};
}

QKeySequence AppSettings::defaultHotkey(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::TranslateSelection: return QKeySequence(QStringLiteral("Ctrl+Alt+T"));
    case HotkeyAction::OcrRegion: return QKeySequence(QStringLiteral("Ctrl+Shift+S"));
    case HotkeyAction::ShowWindow: return QKeySequence();
    }
    return {};
}

QKeySequence AppSettings::hotkey(HotkeyAction action) const
{
    const QString key = hotkeyKey(action);
    if (!m_settings.contains(key))
        return defaultHotkey(action);
    return QKeySequence::fromString(m_settings.value(key).toString(), QKeySequence::PortableText);
}

void AppSettings::setHotkey(HotkeyAction action, const QKeySequence& sequence)
{
    write(hotkeyKey(action), sequence.toString(QKeySequence::PortableText));
}

SelectionAction AppSettings::selectionAction() const
{
    const QString v = m_settings.value(keys::SelectionActionKey, QStringLiteral("replace")).toString();
    if (v == QLatin1String("popup"))
        return SelectionAction::Popup;
    if (v == QLatin1String("copy"))
        return SelectionAction::Copy;
    return SelectionAction::Replace;
}

void AppSettings::setSelectionAction(SelectionAction action)
{
    QString v = QStringLiteral("replace");
    if (action == SelectionAction::Popup)
        v = QStringLiteral("popup");
    else if (action == SelectionAction::Copy)
        v = QStringLiteral("copy");
    write(keys::SelectionActionKey, v);
}

int AppSettings::popupTimeoutSeconds() const
{
    return qBound(0, m_settings.value(keys::PopupTimeout, 15).toInt(), 600);
}

void AppSettings::setPopupTimeoutSeconds(int seconds) { write(keys::PopupTimeout, qBound(0, seconds, 600)); }

int AppSettings::clipboardRestoreDelayMs() const
{
    return qBound(100, m_settings.value(keys::RestoreDelay, 400).toInt(), 5000);
}

void AppSettings::setClipboardRestoreDelayMs(int ms) { write(keys::RestoreDelay, qBound(100, ms, 5000)); }

int AppSettings::copyTimeoutMs() const
{
    return qBound(200, m_settings.value(keys::CopyTimeout, 900).toInt(), 5000);
}

void AppSettings::setCopyTimeoutMs(int ms) { write(keys::CopyTimeout, qBound(200, ms, 5000)); }

QString AppSettings::ocrLanguage() const { return m_settings.value(keys::OcrLanguage).toString(); }

void AppSettings::setOcrLanguage(const QString& tag) { write(keys::OcrLanguage, tag); }

bool AppSettings::ocrMergeLines() const { return m_settings.value(keys::OcrMerge, true).toBool(); }

void AppSettings::setOcrMergeLines(bool merge) { write(keys::OcrMerge, merge); }

int AppSettings::requestTimeoutMs() const
{
    return qBound(2000, m_settings.value(keys::RequestTimeout, 15000).toInt(), 120000);
}

void AppSettings::setRequestTimeoutMs(int ms) { write(keys::RequestTimeout, qBound(2000, ms, 120000)); }

int AppSettings::maxRetries() const { return qBound(0, m_settings.value(keys::MaxRetries, 3).toInt(), 8); }

void AppSettings::setMaxRetries(int retries) { write(keys::MaxRetries, qBound(0, retries, 8)); }

bool AppSettings::autostartConfigured() const { return m_settings.value(keys::AutostartConfigured, false).toBool(); }

void AppSettings::setAutostartConfigured(bool configured) { write(keys::AutostartConfigured, configured); }

bool AppSettings::closeToTray() const { return m_settings.value(keys::CloseToTray, true).toBool(); }

void AppSettings::setCloseToTray(bool toTray) { write(keys::CloseToTray, toTray); }

bool AppSettings::trayHintShown() const { return m_settings.value(keys::TrayHint, false).toBool(); }

void AppSettings::setTrayHintShown(bool shown) { write(keys::TrayHint, shown); }

bool AppSettings::historyEnabled() const { return m_settings.value(keys::HistoryEnabled, true).toBool(); }

void AppSettings::setHistoryEnabled(bool enabled) { write(keys::HistoryEnabled, enabled); }

int AppSettings::historyLimit() const
{
    return qBound(10, m_settings.value(keys::HistoryLimit, 1000).toInt(), 100000);
}

void AppSettings::setHistoryLimit(int limit) { write(keys::HistoryLimit, qBound(10, limit, 100000)); }

QByteArray AppSettings::mainWindowGeometry() const { return m_settings.value(keys::Geometry).toByteArray(); }

void AppSettings::setMainWindowGeometry(const QByteArray& geometry)
{
    // Geometry changes on every move; don't broadcast it.
    m_settings.setValue(keys::Geometry, geometry);
}

} // namespace hypers
