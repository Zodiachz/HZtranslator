#include "ui/TranslatePage.h"

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/Languages.h"
#include "features/FeatureCommon.h"
#include "translation/TranslationService.h"
#include "ui/LanguageComboBox.h"
#include "ui/UiHelpers.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSignalBlocker>

namespace hypers {

TranslatePage::TranslatePage(const AppContext& context, QWidget* parent)
    : QWidget(parent)
    , m_ctx(context)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(18);

    root->addWidget(ui::label(QStringLiteral("Translate"), "pageTitle"));
    root->addWidget(ui::label(QStringLiteral("Type or paste text here - or use the global shortcuts in any app."),
                              "pageSubtitle"));

    QVBoxLayout* body = nullptr;
    root->addWidget(ui::card(QString(), &body), 1);

    // Language bar
    auto* langBar = new QHBoxLayout;
    langBar->setSpacing(10);
    m_source = new LanguageComboBox(true);
    m_target = new LanguageComboBox(false);
    m_swap = ui::button(QStringLiteral("⇄"), "ghost");
    m_swap->setToolTip(QStringLiteral("Swap languages"));
    m_swap->setFixedWidth(44);
    m_provider = ui::label(QString(), "muted");
    langBar->addWidget(m_source, 1);
    langBar->addWidget(m_swap);
    langBar->addWidget(m_target, 1);
    langBar->addSpacing(12);
    langBar->addWidget(m_provider);
    body->addLayout(langBar);

    // Text panes
    auto* panes = new QHBoxLayout;
    panes->setSpacing(12);
    m_input = new QPlainTextEdit;
    m_input->setPlaceholderText(QStringLiteral("Enter text to translate…"));
    m_output = new QPlainTextEdit;
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(QStringLiteral("Translation"));
    panes->addWidget(m_input, 1);
    panes->addWidget(m_output, 1);
    body->addLayout(panes, 1);

    // Action bar
    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_counter = ui::label(QString(), "hint");
    m_status = ui::label(QString(), "muted");
    m_status->setWordWrap(true);
    m_clear = ui::button(QStringLiteral("Clear"), "ghost");
    m_copy = ui::button(QStringLiteral("Copy translation"));
    m_translate = ui::button(QStringLiteral("Translate   Ctrl+Enter"), "accent");
    m_translate->setMinimumWidth(180);
    actions->addWidget(m_counter);
    actions->addSpacing(8);
    actions->addWidget(m_status, 1);
    actions->addWidget(m_clear);
    actions->addWidget(m_copy);
    actions->addWidget(m_translate);
    body->addLayout(actions);

    // Shortcut cheat sheet
    QVBoxLayout* tips = nullptr;
    root->addWidget(ui::card(QStringLiteral("Works everywhere"), &tips));
    auto* tipRow1 = new QHBoxLayout;
    m_selectionHint = ui::kbd(QString());
    tipRow1->addWidget(m_selectionHint);
    QLabel* tip1 = ui::label(QStringLiteral("Select text in any app (Discord, browser, Word…) and press to translate it "
                                            "in place."),
                             "muted");
    tip1->setWordWrap(true);
    tipRow1->addWidget(tip1, 1);
    tips->addLayout(tipRow1);
    auto* tipRowTyped = new QHBoxLayout;
    m_typedHint = ui::kbd(QString());
    tipRowTyped->addWidget(m_typedHint);
    QLabel* tipTyped = ui::label(QStringLiteral("Type in your own language in any text box (chat, email, search…) and "
                                                "press to replace it with the translation, ready to send."),
                                 "muted");
    tipTyped->setWordWrap(true);
    tipRowTyped->addWidget(tipTyped, 1);
    tips->addLayout(tipRowTyped);
    auto* tipRow2 = new QHBoxLayout;
    m_ocrHint = ui::kbd(QString());
    tipRow2->addWidget(m_ocrHint);
    QLabel* tip2 = ui::label(QStringLiteral("Drag a region of the screen - text in images, games and videos is "
                                            "recognised with Windows OCR and translated in a popup."),
                             "muted");
    tip2->setWordWrap(true);
    tipRow2->addWidget(tip2, 1);
    tips->addLayout(tipRow2);

    // Wiring
    connect(m_translate, &QPushButton::clicked, this, &TranslatePage::translateNow);
    auto* shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcut, &QShortcut::activated, this, &TranslatePage::translateNow);
    auto* shortcutEnter = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), this);
    connect(shortcutEnter, &QShortcut::activated, this, &TranslatePage::translateNow);
    connect(m_clear, &QPushButton::clicked, this, [this] {
        if (m_reply)
            m_reply->abort();
        m_input->clear();
        m_output->clear();
        setStatus(QString());
        m_input->setFocus();
    });
    connect(m_copy, &QPushButton::clicked, this, [this] {
        if (!m_output->toPlainText().isEmpty()) {
            QGuiApplication::clipboard()->setText(m_output->toPlainText());
            setStatus(QStringLiteral("Copied to clipboard"));
        }
    });
    connect(m_swap, &QPushButton::clicked, this, &TranslatePage::swapLanguages);
    connect(m_input, &QPlainTextEdit::textChanged, this, &TranslatePage::updateCounter);
    connect(m_source, &LanguageComboBox::codeChanged, this, [this](const QString& code) {
        if (!code.isEmpty())
            m_ctx.settings->setSourceLanguage(code);
    });
    connect(m_target, &LanguageComboBox::codeChanged, this, [this](const QString& code) {
        if (!code.isEmpty())
            m_ctx.settings->setTargetLanguage(code);
    });
    connect(m_ctx.settings, &AppSettings::changed, this, [this](const QString& key) {
        if (key.startsWith(QLatin1String("translation/")))
            syncFromSettings();
        else if (key.startsWith(QLatin1String("hotkeys/")))
            refreshHotkeyHints();
    });
    connect(m_ctx.translation, &TranslationService::activeProviderChanged, this, &TranslatePage::applyProviderFilter);

    applyProviderFilter();
    syncFromSettings();
    refreshHotkeyHints();
    updateCounter();
    setStatus(QString());
}

void TranslatePage::applyProviderFilter()
{
    ITranslationProvider* provider = m_ctx.translation->activeProvider();
    if (!provider)
        return;
    m_source->setFilter([provider](const QString& code) { return provider->supportsSourceLanguage(code); });
    m_target->setFilter([provider](const QString& code) { return provider->supportsTargetLanguage(code); });
    m_provider->setText(QStringLiteral("via %1").arg(provider->displayName()));
    syncFromSettings();
}

void TranslatePage::syncFromSettings()
{
    {
        const QSignalBlocker b1(m_source);
        const QSignalBlocker b2(m_target);
        m_source->setCurrentCode(m_ctx.settings->sourceLanguage());
        m_target->setCurrentCode(m_ctx.settings->targetLanguage());
    }
    if (ITranslationProvider* provider = m_ctx.translation->activeProvider()) {
        if (!provider->supportsTargetLanguage(m_ctx.settings->targetLanguage())) {
            setStatus(QStringLiteral("%1 does not support %2 - pick another target language.")
                          .arg(provider->displayName(), Languages::label(m_ctx.settings->targetLanguage())),
                      true);
        }
    }
}

void TranslatePage::refreshHotkeyHints()
{
    const QString selection = m_ctx.settings->hotkey(HotkeyAction::TranslateSelection).toString(QKeySequence::NativeText);
    const QString typed = m_ctx.settings->hotkey(HotkeyAction::TranslateTyped).toString(QKeySequence::NativeText);
    const QString ocr = m_ctx.settings->hotkey(HotkeyAction::OcrRegion).toString(QKeySequence::NativeText);
    m_selectionHint->setText(selection.isEmpty() ? QStringLiteral("not set") : selection);
    m_typedHint->setText(typed.isEmpty() ? QStringLiteral("not set") : typed);
    m_ocrHint->setText(ocr.isEmpty() ? QStringLiteral("not set") : ocr);
}

void TranslatePage::setSourceText(const QString& text, bool translate)
{
    m_input->setPlainText(text);
    m_output->clear();
    if (translate)
        translateNow();
}

void TranslatePage::focusInput()
{
    m_input->setFocus();
}

void TranslatePage::updateCounter()
{
    const qsizetype count = m_input->toPlainText().size();
    m_counter->setText(QStringLiteral("%1 chars").arg(count));
}

void TranslatePage::setStatus(const QString& text, bool error)
{
    m_status->setObjectName(error ? QStringLiteral("errorText") : QStringLiteral("muted"));
    m_status->setText(text);
    ui::repolish(m_status);
}

void TranslatePage::swapLanguages()
{
    QString newSource = m_target->currentCode();
    QString newTarget = m_source->currentCode();
    if (newTarget == QLatin1String(Languages::kAuto)) {
        // Use the language detected in the last translation, if we can map it.
        newTarget.clear();
        for (const LanguageInfo& lang : Languages::all()) {
            if (!m_lastDetected.isEmpty()
                && (lang.deeplSource.compare(m_lastDetected, Qt::CaseInsensitive) == 0
                    || lang.google.compare(m_lastDetected, Qt::CaseInsensitive) == 0)) {
                newTarget = lang.code;
                break;
            }
        }
        if (newTarget.isEmpty()) {
            setStatus(QStringLiteral("Translate once so the source language is known, then swap."));
            return;
        }
    }
    m_ctx.settings->setSourceLanguage(newSource);
    m_ctx.settings->setTargetLanguage(newTarget);
    syncFromSettings();
    const QString translated = m_output->toPlainText();
    if (!translated.isEmpty())
        setSourceText(translated, true);
}

void TranslatePage::translateNow()
{
    const QString text = m_input->toPlainText();
    if (text.trimmed().isEmpty()) {
        setStatus(QStringLiteral("Enter some text first."));
        return;
    }
    if (m_reply)
        m_reply->abort();

    TranslationRequest request;
    request.text = text;
    request.sourceLanguage = m_source->currentCode();
    request.targetLanguage = m_target->currentCode();

    TranslationReply* reply = m_ctx.translation->translate(request, this);
    m_reply = reply;
    m_translate->setEnabled(false);
    setStatus(QStringLiteral("Translating…"));

    connect(reply, &TranslationReply::retrying, this, [this](int attempt, int delayMs, const QString&) {
        setStatus(QStringLiteral("Connection problem - retry %1 in %2 s…").arg(attempt).arg(qMax(1, delayMs / 1000)));
    });
    connect(reply, &TranslationReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_reply == reply) {
            m_reply = nullptr;
            onFinished(reply);
        }
    });
}

void TranslatePage::onFinished(TranslationReply* reply)
{
    m_translate->setEnabled(true);
    if (!reply->isSuccess()) {
        if (reply->error().kind != TranslationErrorKind::Canceled)
            setStatus(describeError(m_ctx.translation, reply), true);
        return;
    }
    const QString translated = reply->result().translatedText;
    m_output->setPlainText(translated);
    m_output->setLayoutDirection(translated.isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight);
    m_lastDetected = reply->result().detectedSourceLanguage;
    setStatus(QStringLiteral("%1  ·  %2 ms").arg(describeTranslation(m_ctx.translation, reply)).arg(reply->elapsedMs()));
    recordHistory(m_ctx.history, m_ctx.settings, QStringLiteral("manual"), QString(), reply);
}

} // namespace hypers
