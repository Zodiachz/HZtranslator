#include "ui/SettingsPage.h"

#include "core/HistoryStore.h"
#include "core/Languages.h"
#include "core/Logger.h"
#include "platform/ISecretStore.h"
#include "platform/win/AutoStart.h"
#include "platform/win/GlobalHotkeyManager.h"
#include "platform/win/WindowsOcrEngine.h"
#include "translation/TranslationService.h"
#include "ui/HotkeyEdit.h"
#include "ui/LanguageComboBox.h"
#include "ui/UiHelpers.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUrl>

namespace hypers {

namespace {

QFormLayout* makeForm()
{
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    return form;
}

QSpinBox* makeSpin(int min, int max, int step, const QString& suffix)
{
    auto* spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setSuffix(suffix);
    spin->setMaximumWidth(160);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    return spin;
}

QString hotkeyLabel(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::TranslateSelection: return QStringLiteral("Translate selected text");
    case HotkeyAction::OcrRegion: return QStringLiteral("Translate screen region (OCR)");
    case HotkeyAction::ShowWindow: return QStringLiteral("Open HYPERS TRANSLATOR");
    }
    return {};
}

} // namespace

SettingsPage::SettingsPage(const AppContext& context, QWidget* parent)
    : QWidget(parent)
    , m_ctx(context)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(m_scroll);

    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(32, 28, 32, 32);
    root->setSpacing(18);
    root->addWidget(ui::label(QStringLiteral("Settings"), "pageTitle"));
    root->addWidget(ui::label(QStringLiteral("Changes are applied immediately."), "pageSubtitle"));
    root->addWidget(buildTranslationCard());
    root->addWidget(buildHotkeyCard());
    root->addWidget(buildBehaviorCard());
    root->addWidget(buildOcrCard());
    root->addWidget(buildSystemCard());
    root->addStretch(1);
    m_scroll->setWidget(content);

    connect(m_ctx.settings, &AppSettings::changed, this, [this](const QString& key) {
        if (key.startsWith(QLatin1String("translation/")))
            syncLanguageCombos();
    });
}

// ---------------------------------------------------------------------------
// Translation
// ---------------------------------------------------------------------------

QWidget* SettingsPage::buildTranslationCard()
{
    QVBoxLayout* body = nullptr;
    QFrame* card = ui::card(QStringLiteral("Translation"), &body);
    QFormLayout* form = makeForm();
    body->addLayout(form);

    m_provider = new QComboBox;
    for (ITranslationProvider* provider : m_ctx.translation->providers())
        m_provider->addItem(provider->displayName(), provider->id());
    m_provider->setCurrentIndex(qMax(0, m_provider->findData(m_ctx.settings->providerId())));
    connect(m_provider, &QComboBox::currentIndexChanged, this, [this](int) {
        m_ctx.settings->setProviderId(m_provider->currentData().toString());
    });
    form->addRow(QStringLiteral("Provider"), m_provider);

    m_target = new LanguageComboBox(false);
    m_source = new LanguageComboBox(true);
    connect(m_target, &LanguageComboBox::codeChanged, this, [this](const QString& code) {
        if (!code.isEmpty())
            m_ctx.settings->setTargetLanguage(code);
    });
    connect(m_source, &LanguageComboBox::codeChanged, this, [this](const QString& code) {
        if (!code.isEmpty())
            m_ctx.settings->setSourceLanguage(code);
    });
    form->addRow(QStringLiteral("Translate into"), m_target);
    form->addRow(QStringLiteral("Source language"), m_source);

    addKeyRow(form, QStringLiteral("deepl"), QStringLiteral("DeepL API key"),
              QStringLiteral("Free or Pro key from deepl.com/your-account/keys. Free keys end in “:fx”."));
    addKeyRow(form, QStringLiteral("google"), QStringLiteral("Google Cloud API key"),
              QStringLiteral("API key from console.cloud.google.com with the Cloud Translation API enabled."));

    body->addWidget(ui::label(QStringLiteral("Keys are encrypted by Windows (Credential Manager, DPAPI) and never "
                                             "written to settings files or logs."),
                              "hint"));

    QFormLayout* network = makeForm();
    QSpinBox* timeout = makeSpin(2, 120, 1, QStringLiteral(" s"));
    timeout->setValue(m_ctx.settings->requestTimeoutMs() / 1000);
    connect(timeout, &QSpinBox::valueChanged, this, [this](int s) { m_ctx.settings->setRequestTimeoutMs(s * 1000); });
    network->addRow(QStringLiteral("Request timeout"), timeout);
    QSpinBox* retries = makeSpin(0, 8, 1, QString());
    retries->setValue(m_ctx.settings->maxRetries());
    connect(retries, &QSpinBox::valueChanged, this, [this](int n) { m_ctx.settings->setMaxRetries(n); });
    network->addRow(QStringLiteral("Retries on network errors"), retries);
    body->addLayout(network);

    syncLanguageCombos();
    connect(m_ctx.translation, &TranslationService::activeProviderChanged, this, [this] { syncLanguageCombos(); });
    return card;
}

void SettingsPage::addKeyRow(QFormLayout* form, const QString& providerId, const QString& label, const QString& hint)
{
    const std::size_t index = m_keys.size();
    KeyRow row;
    row.providerId = providerId;

    auto* container = new QWidget;
    auto* v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto* h = new QHBoxLayout;
    h->setSpacing(8);
    row.edit = new QLineEdit;
    row.edit->setEchoMode(QLineEdit::Password);
    row.edit->setClearButtonEnabled(true);
    row.show = ui::button(QStringLiteral("Show"), "ghost");
    row.show->setCheckable(true);
    row.save = ui::button(QStringLiteral("Save"), "accent");
    row.test = ui::button(QStringLiteral("Test"));
    row.remove = ui::button(QStringLiteral("Remove"), "ghost");
    h->addWidget(row.edit, 1);
    h->addWidget(row.show);
    h->addWidget(row.save);
    h->addWidget(row.test);
    h->addWidget(row.remove);
    v->addLayout(h);
    v->addWidget(ui::label(hint, "hint"));
    row.status = ui::label(QString(), "muted");
    row.status->setWordWrap(true);
    row.status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(row.status);
    form->addRow(label, container);

    connect(row.show, &QPushButton::toggled, this, [this, index](bool shown) {
        m_keys[index].edit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        m_keys[index].show->setText(shown ? QStringLiteral("Hide") : QStringLiteral("Show"));
    });
    connect(row.edit, &QLineEdit::textChanged, this, [this, index](const QString& text) {
        m_keys[index].save->setEnabled(!text.trimmed().isEmpty());
    });
    connect(row.edit, &QLineEdit::returnPressed, this, [this, index] { saveKey(index); });
    connect(row.save, &QPushButton::clicked, this, [this, index] { saveKey(index); });
    connect(row.test, &QPushButton::clicked, this, [this, index] { testKey(index); });
    connect(row.remove, &QPushButton::clicked, this, [this, index] { removeKey(index); });

    m_keys.push_back(row);
    refreshKeyState(index);
}

void SettingsPage::refreshKeyState(std::size_t index)
{
    KeyRow& row = m_keys[index];
    const ITranslationProvider* provider = m_ctx.translation->provider(row.providerId);
    const bool hasKey = provider && provider->hasApiKey();
    row.edit->setPlaceholderText(hasKey ? QStringLiteral("••••••••••••  saved - type a new key to replace it")
                                        : QStringLiteral("Paste your API key"));
    row.save->setEnabled(!row.edit->text().trimmed().isEmpty());
    row.test->setEnabled(hasKey);
    row.remove->setEnabled(hasKey);
}

void SettingsPage::saveKey(std::size_t index)
{
    KeyRow& row = m_keys[index];
    const QString key = row.edit->text().trimmed();
    if (key.isEmpty())
        return;
    QString error;
    if (!m_ctx.secrets->write(apiKeySecretName(row.providerId), key, &error)) {
        ui::setStatus(row.status, QStringLiteral("Could not save the key: %1").arg(error), true);
        return;
    }
    if (ITranslationProvider* provider = m_ctx.translation->provider(row.providerId))
        provider->setApiKey(key);
    m_ctx.translation->clearCache();
    row.edit->clear();
    row.show->setChecked(false);
    refreshKeyState(index);
    ui::setStatus(row.status, QStringLiteral("Saved. Testing…"), false);
    testKey(index);
}

void SettingsPage::testKey(std::size_t index)
{
    KeyRow& row = m_keys[index];
    ITranslationProvider* provider = m_ctx.translation->provider(row.providerId);
    if (!provider || !provider->hasApiKey())
        return;
    if (row.testReply)
        row.testReply->abort();

    TranslationRequest request;
    request.text = QStringLiteral("Hello, world!");
    request.sourceLanguage = QStringLiteral("en-US");
    request.targetLanguage = provider->supportsTargetLanguage(m_ctx.settings->targetLanguage())
            && !m_ctx.settings->targetLanguage().startsWith(QLatin1String("en"))
        ? m_ctx.settings->targetLanguage()
        : QStringLiteral("de");

    m_ctx.translation->clearCache();
    TranslationReply* reply = m_ctx.translation->translateWith(row.providerId, request, this);
    row.testReply = reply;
    row.test->setEnabled(false);
    ui::setStatus(row.status, QStringLiteral("Testing…"), false);
    connect(reply, &TranslationReply::finished, this, [this, index, reply] {
        reply->deleteLater();
        KeyRow& r = m_keys[index];
        r.test->setEnabled(true);
        if (reply->isSuccess()) {
            ui::setStatus(r.status,
                          QStringLiteral("✓ Works - “Hello, world!” → “%1” (%2 ms)")
                              .arg(reply->result().translatedText)
                              .arg(reply->elapsedMs()),
                          false);
        } else if (reply->error().kind != TranslationErrorKind::Canceled) {
            ui::setStatus(r.status, QStringLiteral("✗ %1").arg(reply->error().userMessage()), true);
        }
    });
}

void SettingsPage::removeKey(std::size_t index)
{
    KeyRow& row = m_keys[index];
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Remove API key"),
        QStringLiteral("Remove the saved %1 key from Windows Credential Manager?").arg(m_provider->itemText(
            m_provider->findData(row.providerId))),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;
    QString error;
    if (!m_ctx.secrets->remove(apiKeySecretName(row.providerId), &error)) {
        ui::setStatus(row.status, QStringLiteral("Could not remove the key: %1").arg(error), true);
        return;
    }
    if (ITranslationProvider* provider = m_ctx.translation->provider(row.providerId))
        provider->setApiKey(QString());
    m_ctx.translation->clearCache();
    refreshKeyState(index);
    ui::setStatus(row.status, QStringLiteral("Key removed."), false);
}

void SettingsPage::syncLanguageCombos()
{
    ITranslationProvider* provider = m_ctx.translation->provider(m_ctx.settings->providerId());
    if (provider) {
        m_target->setFilter([provider](const QString& c) { return provider->supportsTargetLanguage(c); });
        m_source->setFilter([provider](const QString& c) { return provider->supportsSourceLanguage(c); });
    }
    const QSignalBlocker b1(m_target);
    const QSignalBlocker b2(m_source);
    const QSignalBlocker b3(m_provider);
    m_target->setCurrentCode(m_ctx.settings->targetLanguage());
    m_source->setCurrentCode(m_ctx.settings->sourceLanguage());
    m_provider->setCurrentIndex(qMax(0, m_provider->findData(m_ctx.settings->providerId())));
}

void SettingsPage::focusApiKey()
{
    for (KeyRow& row : m_keys) {
        if (row.providerId == m_ctx.settings->providerId()) {
            m_scroll->ensureWidgetVisible(row.edit, 0, 120);
            row.edit->setFocus();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Hotkeys
// ---------------------------------------------------------------------------

QWidget* SettingsPage::buildHotkeyCard()
{
    QVBoxLayout* body = nullptr;
    QFrame* card = ui::card(QStringLiteral("Global shortcuts"), &body);
    body->addWidget(ui::label(QStringLiteral("Click a field and press the new combination. Backspace clears it, "
                                             "Esc cancels."),
                              "hint"));
    QFormLayout* form = makeForm();
    body->addLayout(form);

    for (HotkeyAction action :
         {HotkeyAction::TranslateSelection, HotkeyAction::OcrRegion, HotkeyAction::ShowWindow}) {
        HotkeyRow row{action, new HotkeyEdit};
        row.edit->setKeySequence(m_ctx.settings->hotkey(action));
        row.edit->setMaximumWidth(260);

        auto* line = new QHBoxLayout;
        line->setSpacing(8);
        line->addWidget(row.edit);
        QPushButton* reset = ui::button(QStringLiteral("Default"), "ghost");
        reset->setToolTip(AppSettings::defaultHotkey(action).isEmpty()
                              ? QStringLiteral("No default - disable")
                              : AppSettings::defaultHotkey(action).toString(QKeySequence::NativeText));
        line->addWidget(reset);
        line->addStretch(1);
        form->addRow(hotkeyLabel(action), line);

        connect(row.edit, &HotkeyEdit::recordingChanged, this,
                [this](bool recording) { m_ctx.hotkeys->setSuspended(recording); });
        connect(row.edit, &HotkeyEdit::keySequenceEdited, this,
                [this, row](const QKeySequence& sequence) { applyHotkey(row, sequence); });
        connect(reset, &QPushButton::clicked, this, [this, row] {
            const QKeySequence def = AppSettings::defaultHotkey(row.action);
            row.edit->setKeySequence(def);
            applyHotkey(row, def);
        });
        m_hotkeys.push_back(row);
    }

    m_hotkeyStatus = ui::label(QString(), "muted");
    m_hotkeyStatus->setWordWrap(true);
    m_hotkeyStatus->hide();
    body->addWidget(m_hotkeyStatus);
    return card;
}

void SettingsPage::applyHotkey(const HotkeyRow& row, const QKeySequence& sequence)
{
    QString error;
    if (!m_ctx.hotkeys->setHotkey(row.action, sequence, &error)) {
        row.edit->setKeySequence(m_ctx.settings->hotkey(row.action)); // revert
        ui::setStatus(m_hotkeyStatus, error, true);
        return;
    }
    m_ctx.settings->setHotkey(row.action, sequence);
    ui::setStatus(m_hotkeyStatus,
                  sequence.isEmpty()
                      ? QStringLiteral("“%1” disabled.").arg(hotkeyLabel(row.action))
                      : QStringLiteral("“%1” is now %2.")
                            .arg(hotkeyLabel(row.action), sequence.toString(QKeySequence::NativeText)),
                  false);
}

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------

QWidget* SettingsPage::buildBehaviorCard()
{
    QVBoxLayout* body = nullptr;
    QFrame* card = ui::card(QStringLiteral("Selected text"), &body);
    QFormLayout* form = makeForm();
    body->addLayout(form);

    auto* action = new QComboBox;
    action->addItem(QStringLiteral("Replace the selection with the translation"),
                    static_cast<int>(SelectionAction::Replace));
    action->addItem(QStringLiteral("Show the translation in a popup"), static_cast<int>(SelectionAction::Popup));
    action->addItem(QStringLiteral("Copy the translation to the clipboard"), static_cast<int>(SelectionAction::Copy));
    action->setCurrentIndex(action->findData(static_cast<int>(m_ctx.settings->selectionAction())));
    connect(action, &QComboBox::currentIndexChanged, this, [this, action](int) {
        m_ctx.settings->setSelectionAction(static_cast<SelectionAction>(action->currentData().toInt()));
    });
    form->addRow(QStringLiteral("When the shortcut is pressed"), action);

    QSpinBox* popupTimeout = makeSpin(0, 600, 1, QStringLiteral(" s"));
    popupTimeout->setSpecialValueText(QStringLiteral("Never"));
    popupTimeout->setValue(m_ctx.settings->popupTimeoutSeconds());
    connect(popupTimeout, &QSpinBox::valueChanged, this,
            [this](int s) { m_ctx.settings->setPopupTimeoutSeconds(s); });
    form->addRow(QStringLiteral("Hide popups after"), popupTimeout);

    QSpinBox* copyTimeout = makeSpin(200, 5000, 100, QStringLiteral(" ms"));
    copyTimeout->setValue(m_ctx.settings->copyTimeoutMs());
    copyTimeout->setToolTip(QStringLiteral("How long to wait for the app to copy the selection. Increase for slow apps."));
    connect(copyTimeout, &QSpinBox::valueChanged, this, [this](int ms) { m_ctx.settings->setCopyTimeoutMs(ms); });
    form->addRow(QStringLiteral("Copy timeout"), copyTimeout);

    QSpinBox* restoreDelay = makeSpin(100, 5000, 50, QStringLiteral(" ms"));
    restoreDelay->setValue(m_ctx.settings->clipboardRestoreDelayMs());
    restoreDelay->setToolTip(QStringLiteral("Wait this long after pasting before your original clipboard is restored. "
                                            "Increase if an app pastes the old clipboard content."));
    connect(restoreDelay, &QSpinBox::valueChanged, this,
            [this](int ms) { m_ctx.settings->setClipboardRestoreDelayMs(ms); });
    form->addRow(QStringLiteral("Clipboard restore delay"), restoreDelay);

    body->addWidget(ui::label(QStringLiteral("Your clipboard is saved before the shortcut copies the selection and put "
                                             "back afterwards. The temporary translation is excluded from Windows "
                                             "clipboard history."),
                              "hint"));
    return card;
}

// ---------------------------------------------------------------------------
// OCR
// ---------------------------------------------------------------------------

QWidget* SettingsPage::buildOcrCard()
{
    QVBoxLayout* body = nullptr;
    QFrame* card = ui::card(QStringLiteral("Screen OCR"), &body);
    QFormLayout* form = makeForm();
    body->addLayout(form);

    auto* languageRow = new QHBoxLayout;
    m_ocrLanguage = new QComboBox;
    languageRow->addWidget(m_ocrLanguage, 1);
    QPushButton* refresh = ui::button(QStringLiteral("Refresh"), "ghost");
    refresh->setToolTip(QStringLiteral("Reload after installing a language pack"));
    languageRow->addWidget(refresh);
    form->addRow(QStringLiteral("Text language"), languageRow);
    connect(refresh, &QPushButton::clicked, this, &SettingsPage::reloadOcrLanguages);
    connect(m_ocrLanguage, &QComboBox::currentIndexChanged, this, [this](int) {
        m_ctx.settings->setOcrLanguage(m_ocrLanguage->currentData().toString());
    });

    auto* merge = new QCheckBox(QStringLiteral("Join wrapped lines into paragraphs (better translations)"));
    merge->setChecked(m_ctx.settings->ocrMergeLines());
    connect(merge, &QCheckBox::toggled, this, [this](bool on) { m_ctx.settings->setOcrMergeLines(on); });
    form->addRow(QString(), merge);

    m_ocrHint = ui::label(QString(), "hint");
    m_ocrHint->setWordWrap(true);
    m_ocrHint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->addWidget(m_ocrHint);

    reloadOcrLanguages();
    return card;
}

void SettingsPage::reloadOcrLanguages()
{
    const QList<OcrLanguage> languages = WindowsOcrEngine::availableLanguages();
    const QString current = m_ctx.settings->ocrLanguage();
    {
        const QSignalBlocker blocker(m_ocrLanguage);
        m_ocrLanguage->clear();
        m_ocrLanguage->addItem(QStringLiteral("Automatic (Windows display languages)"), QString());
        for (const OcrLanguage& lang : languages)
            m_ocrLanguage->addItem(QStringLiteral("%1  (%2)").arg(lang.displayName, lang.tag), lang.tag);
        const int index = m_ocrLanguage->findData(current);
        m_ocrLanguage->setCurrentIndex(index >= 0 ? index : 0);
    }

    QStringList tags;
    for (const OcrLanguage& lang : languages)
        tags << lang.tag;
    if (languages.isEmpty()) {
        m_ocrHint->setText(QStringLiteral("No OCR languages are installed. ") + WindowsOcrEngine::installHint());
    } else {
        m_ocrHint->setText(QStringLiteral("Installed OCR languages: %1. Windows OCR reads one language/script per "
                                          "capture; to add another, install its language pack.")
                               .arg(tags.join(QStringLiteral(", "))));
    }
}

// ---------------------------------------------------------------------------
// System
// ---------------------------------------------------------------------------

QWidget* SettingsPage::buildSystemCard()
{
    QVBoxLayout* body = nullptr;
    QFrame* card = ui::card(QStringLiteral("System & privacy"), &body);

    m_autostart = new QCheckBox(QStringLiteral("Start with Windows"));
    m_autostart->setChecked(autostart::isEnabled());
    connect(m_autostart, &QCheckBox::toggled, this, [this](bool on) {
        QString error;
        if (!autostart::setEnabled(on, &error)) {
            const QSignalBlocker blocker(m_autostart);
            m_autostart->setChecked(!on);
            QMessageBox::warning(this, QStringLiteral("Start with Windows"), error);
        }
    });
    body->addWidget(m_autostart);

    auto* minimized = new QCheckBox(QStringLiteral("When started with Windows, stay in the tray"));
    minimized->setChecked(m_ctx.settings->startMinimized());
    connect(minimized, &QCheckBox::toggled, this, [this](bool on) { m_ctx.settings->setStartMinimized(on); });
    body->addWidget(minimized);

    auto* closeToTray = new QCheckBox(QStringLiteral("Closing the window keeps HYPERS TRANSLATOR running in the tray"));
    closeToTray->setChecked(m_ctx.settings->closeToTray());
    connect(closeToTray, &QCheckBox::toggled, this, [this](bool on) { m_ctx.settings->setCloseToTray(on); });
    body->addWidget(closeToTray);

    auto* history = new QCheckBox(QStringLiteral("Save translation history on this PC"));
    history->setChecked(m_ctx.settings->historyEnabled());
    connect(history, &QCheckBox::toggled, this, [this](bool on) { m_ctx.settings->setHistoryEnabled(on); });
    body->addWidget(history);

    QFormLayout* form = makeForm();
    QSpinBox* limit = makeSpin(10, 100000, 100, QStringLiteral(" entries"));
    limit->setValue(m_ctx.settings->historyLimit());
    connect(limit, &QSpinBox::valueChanged, this, [this](int n) { m_ctx.settings->setHistoryLimit(n); });
    form->addRow(QStringLiteral("Keep at most"), limit);
    body->addLayout(form);

    auto* buttons = new QHBoxLayout;
    QPushButton* clearHistory = ui::button(QStringLiteral("Clear history…"), "danger");
    connect(clearHistory, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("Clear history"),
                                  QStringLiteral("Delete all translation history from this PC?"),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
            == QMessageBox::Yes) {
            m_ctx.history->clear();
        }
    });
    QPushButton* logs = ui::button(QStringLiteral("Open log folder"), "ghost");
    connect(logs, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(Logger::logFilePath()).absolutePath()));
    });
    buttons->addWidget(clearHistory);
    buttons->addWidget(logs);
    buttons->addStretch(1);
    body->addLayout(buttons);
    return card;
}

} // namespace hypers
