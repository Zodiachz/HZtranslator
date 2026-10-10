#pragma once

#include "core/AppSettings.h"
#include "platform/win/Clipboard.h"
#include "platform/win/KeyMapping.h"
#include "translation/TranslationService.h"

#include <QObject>
#include <QPointer>
#include <QRect>

#include <memory>
#include <optional>

#include <windows.h>

namespace hypers {

class HistoryStore;
class MessageWindow;
class ResultPopup;

// "Select text anywhere, press the hotkey, get it translated in place."
//
// 1. wait until the user releases the hotkey (otherwise the target app would
//    see Ctrl+Alt+C instead of Ctrl+C),
// 2. snapshot the clipboard, send Ctrl+C, wait for the clipboard to change,
//    read the text and immediately put the user's clipboard back,
// 3. translate asynchronously,
// 4. depending on the configured action: paste the translation over the
//    still-selected text (Ctrl+V, then restore the clipboard again), show it
//    in a popup, or copy it.
class SelectionTranslator : public QObject {
    Q_OBJECT
public:
    SelectionTranslator(AppSettings* settings, TranslationService* service, HistoryStore* history,
                        ResultPopup* popup, QObject* parent = nullptr);
    ~SelectionTranslator() override;

    void trigger(std::optional<NativeHotkey> hotkey);
    bool isBusy() const { return m_job.has_value(); }

Q_SIGNALS:
    void notification(const QString& title, const QString& message, bool isError);

private:
    struct Job {
        quint64 id = 0;
        HWND target = nullptr;
        QString application;
        SelectionAction action = SelectionAction::Replace;
        QRect anchor;
        QString sourceText;
        std::optional<clipboard::Snapshot> snapshot;
        QPointer<TranslationReply> reply;
    };

    // True if job `id` is still the current one (callbacks may outlive their job).
    bool isCurrent(quint64 id) const { return m_job && m_job->id == id; }
    void copySelection();
    void readCopiedText();
    void translate(const QString& text);
    void onTranslated(TranslationReply* reply);
    void pasteTranslation(const QString& translation, const QString& title);
    void showFallbackPopup(const QString& title, const QString& translation, const QString& reason);
    void restoreClipboard();
    void fail(const QString& message);
    void finish();
    QRect anchorFor(HWND target) const;
    HWND owner() const;

    AppSettings* m_settings;
    TranslationService* m_service;
    HistoryStore* m_history;
    ResultPopup* m_popup;
    std::unique_ptr<MessageWindow> m_clipboardOwner;
    std::optional<Job> m_job;
    quint64 m_nextJobId = 1;
};

} // namespace hypers
