#include "features/SelectionTranslator.h"

#include "core/AsyncWait.h"
#include "core/HistoryStore.h"
#include "features/FeatureCommon.h"
#include "platform/win/InputSimulator.h"
#include "platform/win/MessageWindow.h"
#include "platform/win/WindowUtils.h"
#include "translation/TranslationService.h"
#include "ui/ResultPopup.h"

#include <QCursor>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcSelection, "hypers.selection")

namespace hypers {

namespace {
constexpr int kHotkeyReleaseTimeoutMs = 1500;
constexpr int kClipboardSettleMs = 40;   // some apps write several formats in sequence
constexpr int kModifierWaitBeforePasteMs = 1500;
constexpr int kSelectAllSettleMs = 60;   // let the app apply Ctrl+A before Ctrl+C

QString preview(const QString& text, int maxChars = 90)
{
    QString simplified = text.simplified();
    if (simplified.size() > maxChars)
        simplified = simplified.left(maxChars - 1) + QChar(0x2026);
    return simplified;
}
} // namespace

SelectionTranslator::SelectionTranslator(AppSettings* settings, TranslationService* service, HistoryStore* history,
                                         ResultPopup* popup, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_service(service)
    , m_history(history)
    , m_popup(popup)
    , m_clipboardOwner(std::make_unique<MessageWindow>())
{
}

SelectionTranslator::~SelectionTranslator()
{
    if (m_job && m_job->reply)
        m_job->reply->abort();
}

HWND SelectionTranslator::owner() const
{
    return m_clipboardOwner ? m_clipboardOwner->hwnd() : nullptr;
}

QRect SelectionTranslator::anchorFor(HWND target) const
{
    if (const std::optional<QRect> caret = winutil::caretRectForWindow(target))
        return *caret;
    const QPoint cursor = QCursor::pos();
    return QRect(cursor - QPoint(4, 4), QSize(8, 8));
}

void SelectionTranslator::trigger(std::optional<NativeHotkey> hotkey, Mode mode)
{
    if (m_job) {
        qCInfo(lcSelection) << "Ignoring hotkey - a translation is already in progress";
        return;
    }
    const HWND target = winutil::foregroundWindow();
    if (!target)
        return;

    const QString app = winutil::processNameForWindow(target);
    if (winutil::isWindowMoreElevatedThanUs(target) && mode == Mode::Typed) {
        // Screen OCR can't help here: the point is to paste into the window.
        Q_EMIT notification(
            QStringLiteral("%1 runs as administrator").arg(app.isEmpty() ? QStringLiteral("This window") : app),
            QStringLiteral("Windows blocks keystrokes to admin apps, so the text can't be replaced. Run HYPERS "
                           "TRANSLATOR as administrator to translate what you type there."),
            true);
        return;
    }
    if (winutil::isWindowMoreElevatedThanUs(target)) {
        // UIPI blocks our Ctrl+C as well, so the selection can't be read at all.
        // Instead of a dead end, hand over to screen OCR so the user can drag over the text.
        Q_EMIT notification(
            QStringLiteral("%1 runs as administrator").arg(app.isEmpty() ? QStringLiteral("This window") : app),
            QStringLiteral("Windows blocks keystrokes to admin apps, so switching to screen OCR: drag over the text "
                           "to translate it. (Running HYPERS TRANSLATOR as administrator enables in-place replace.)"),
            false);
        Q_EMIT fallbackToScreenOcrRequested();
        return;
    }

    Job job;
    job.id = m_nextJobId++;
    job.target = target;
    job.mode = mode;
    job.application = app;
    // Typed text is always replaced: the point is to send it in the other language.
    job.action = (mode == Mode::Typed) ? SelectionAction::Replace : m_settings->selectionAction();
    job.anchor = anchorFor(target);
    m_job = std::move(job);
    const quint64 id = m_job->id;
    qCInfo(lcSelection) << (mode == Mode::Typed ? "Translate typed text in" : "Translate selection in") << app;

    const unsigned int hotkeyVk = hotkey ? hotkey->virtualKey : 0;
    waitUntil(
        this, [hotkeyVk] { return !input::anyKeyDown({hotkeyVk}); }, kHotkeyReleaseTimeoutMs,
        [this, id](bool released) {
            if (!isCurrent(id))
                return;
            if (!released) {
                qCInfo(lcSelection) << "Hotkey still held - releasing modifiers synthetically";
                input::releaseModifiers();
            }
            if (m_job->mode == Mode::Typed)
                selectAll();
            else
                copySelection();
        });
}

void SelectionTranslator::selectAll()
{
    if (winutil::foregroundWindow() != m_job->target) {
        finish(); // the user switched windows while we waited - do nothing
        return;
    }
    if (!input::sendCtrlChord('A')) {
        fail(QStringLiteral("Windows rejected the simulated Ctrl+A."));
        return;
    }
    const quint64 id = m_job->id;
    QTimer::singleShot(kSelectAllSettleMs, this, [this, id] {
        if (isCurrent(id))
            copySelection();
    });
}

void SelectionTranslator::copySelection()
{
    if (winutil::foregroundWindow() != m_job->target) {
        finish(); // the user switched windows while we waited - do nothing
        return;
    }

    QString error;
    std::optional<clipboard::Snapshot> snapshot = clipboard::Snapshot::capture(owner(), &error);
    if (!snapshot) {
        fail(error);
        return;
    }
    m_job->snapshot = std::move(snapshot);

    const DWORD before = clipboard::sequenceNumber();
    if (!input::sendCtrlChord('C')) {
        fail(QStringLiteral("Windows rejected the simulated Ctrl+C."));
        return;
    }

    const quint64 id = m_job->id;
    waitUntil(
        this, [before] { return clipboard::sequenceNumber() != before; }, m_settings->copyTimeoutMs(),
        [this, id](bool changed) {
            if (!isCurrent(id))
                return;
            if (!changed) {
                m_job->snapshot.reset(); // clipboard untouched - nothing to restore
                if (m_job->mode == Mode::Typed)
                    Q_EMIT notification(QStringLiteral("Nothing to translate"),
                                        QStringLiteral("Click into a text field and type something first. (The app "
                                                       "may also not support Ctrl+A / Ctrl+C.)"),
                                        false);
                else
                    Q_EMIT notification(QStringLiteral("No text selected"),
                                        QStringLiteral("Select some text first. (The app may also not support "
                                                       "Ctrl+C.)"),
                                        false);
                finish();
                return;
            }
            QTimer::singleShot(kClipboardSettleMs, this, [this, id] {
                if (isCurrent(id))
                    readCopiedText();
            });
        });
}

void SelectionTranslator::readCopiedText()
{
    const std::optional<QString> text = clipboard::readText(owner());
    restoreClipboard(); // give the user their clipboard back right away

    if (!text || text->trimmed().isEmpty()) {
        Q_EMIT notification(QStringLiteral("Nothing to translate"),
                            m_job->mode == Mode::Typed ? QStringLiteral("The text field is empty.")
                                                       : QStringLiteral("The selection doesn't contain any text."),
                            false);
        finish();
        return;
    }
    translate(*text);
}

void SelectionTranslator::translate(const QString& text)
{
    m_job->sourceText = text;
    m_popup->showLoading(QStringLiteral("Translating"), m_job->anchor);

    TranslationRequest request;
    request.text = text;
    request.sourceLanguage = m_settings->sourceLanguage();
    request.targetLanguage =
        (m_job->mode == Mode::Typed) ? m_settings->typingTargetLanguage() : m_settings->targetLanguage();

    TranslationReply* reply = m_service->translate(request, this);
    m_job->reply = reply;
    const quint64 id = m_job->id;
    connect(reply, &TranslationReply::retrying, this, [this, id](int attempt, int, const QString&) {
        if (isCurrent(id))
            m_popup->updateStatus(QStringLiteral("Connection problem - retrying (attempt %1)").arg(attempt));
    });
    connect(reply, &TranslationReply::finished, this, [this, reply, id] {
        reply->deleteLater();
        if (isCurrent(id))
            onTranslated(reply);
    });
}

void SelectionTranslator::onTranslated(TranslationReply* reply)
{
    if (!reply->isSuccess()) {
        if (reply->error().kind == TranslationErrorKind::Canceled) {
            m_popup->hide();
        } else {
            m_popup->showError(describeError(m_service, reply), m_job->anchor, m_job->sourceText);
        }
        finish();
        return;
    }

    recordHistory(m_history, m_settings,
                  m_job->mode == Mode::Typed ? QStringLiteral("typed") : QStringLiteral("selection"),
                  m_job->application, reply);
    const QString translation = reply->result().translatedText;
    const QString title = describeTranslation(m_service, reply);

    switch (m_job->action) {
    case SelectionAction::Popup:
        m_popup->showResult(title, m_job->sourceText, translation, m_job->anchor);
        finish();
        return;
    case SelectionAction::Copy: {
        m_popup->hide();
        QString error;
        if (clipboard::setText(owner(), translation, false, &error))
            Q_EMIT notification(QStringLiteral("Translation copied"), preview(translation), false);
        else
            showFallbackPopup(title, translation, error);
        finish();
        return;
    }
    case SelectionAction::Replace:
        m_popup->hide();
        pasteTranslation(translation, title);
        return;
    }
}

void SelectionTranslator::pasteTranslation(const QString& translation, const QString& title)
{
    const quint64 id = m_job->id;
    // The user may be holding keys again (typing, another shortcut); a Ctrl+V
    // mixed with e.g. Shift would become Ctrl+Shift+V.
    waitUntil(
        this, [] { return !input::anyModifierDown(); }, kModifierWaitBeforePasteMs,
        [this, id, translation, title](bool) {
            if (!isCurrent(id))
                return;
            if (winutil::foregroundWindow() != m_job->target) {
                showFallbackPopup(title, translation,
                                  QStringLiteral("You switched windows, so the text was not replaced."));
                finish();
                return;
            }

            QString error;
            // Snapshot again: the user may have copied something new meanwhile.
            m_job->snapshot = clipboard::Snapshot::capture(owner(), &error);
            if (!clipboard::setText(owner(), translation, true, &error)) {
                showFallbackPopup(title, translation, error);
                finish();
                return;
            }
            const DWORD ours = clipboard::sequenceNumber();
            if (!input::sendCtrlChord('V')) {
                restoreClipboard();
                showFallbackPopup(title, translation, QStringLiteral("Windows rejected the simulated Ctrl+V."));
                finish();
                return;
            }

            // The target reads the clipboard asynchronously after Ctrl+V; only
            // restore once it has had time to paste, and only if nobody else
            // has written to the clipboard in the meantime.
            QTimer::singleShot(m_settings->clipboardRestoreDelayMs(), this, [this, id, ours] {
                if (!isCurrent(id))
                    return;
                if (clipboard::sequenceNumber() == ours)
                    restoreClipboard();
                else
                    qCInfo(lcSelection) << "Clipboard changed after paste - not restoring";
                qCInfo(lcSelection) << (m_job->mode == Mode::Typed ? "Replaced typed text in" : "Replaced selection in")
                                    << m_job->application;
                finish();
            });
        });
}

void SelectionTranslator::showFallbackPopup(const QString& title, const QString& translation, const QString& reason)
{
    qCInfo(lcSelection) << "Falling back to popup:" << reason;
    m_popup->showResult(title + QStringLiteral("  ·  not replaced"), m_job->sourceText, translation, m_job->anchor);
    Q_EMIT notification(QStringLiteral("Shown instead of replaced"), reason, false);
}

void SelectionTranslator::restoreClipboard()
{
    if (!m_job || !m_job->snapshot)
        return;
    QString error;
    if (!m_job->snapshot->restore(owner(), &error))
        qCWarning(lcSelection) << "Clipboard restore failed:" << error;
    m_job->snapshot.reset();
}

void SelectionTranslator::fail(const QString& message)
{
    qCWarning(lcSelection) << "Selection translation failed:" << message;
    restoreClipboard();
    Q_EMIT notification(QStringLiteral("Translation failed"), message, true);
    finish();
}

void SelectionTranslator::finish()
{
    m_job.reset();
}

} // namespace hypers
