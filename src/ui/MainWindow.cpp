#include "ui/MainWindow.h"

#include "Version.h"
#include "core/AppSettings.h"
#include "platform/win/WindowUtils.h"
#include "translation/TranslationService.h"
#include "ui/HistoryPage.h"
#include "ui/SettingsPage.h"
#include "ui/Theme.h"
#include "ui/TranslatePage.h"
#include "ui/UiHelpers.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QScreen>
#include <QStackedWidget>

namespace hypers {

MainWindow::MainWindow(const AppContext& context, QWidget* parent)
    : QMainWindow(parent)
    , m_ctx(context)
{
    setWindowTitle(QStringLiteral(HYPERS_APP_NAME));
    setWindowIcon(theme::appIcon());
    setMinimumSize(900, 620);

    auto* central = new QWidget;
    central->setObjectName(QStringLiteral("central"));
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget;
    m_translatePage = new TranslatePage(m_ctx);
    m_historyPage = new HistoryPage(m_ctx);
    m_settingsPage = new SettingsPage(m_ctx);
    m_stack->addWidget(m_translatePage);
    m_stack->addWidget(m_historyPage);
    m_stack->addWidget(m_settingsPage);
    m_stack->addWidget(buildAboutPage());

    layout->addWidget(buildSidebar());
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    connect(m_historyPage, &HistoryPage::openInTranslator, this, [this](const QString& text) {
        m_translatePage->setSourceText(text, true);
        showPage(Page::Translate);
    });
    connect(m_ctx.translation, &TranslationService::activeProviderChanged, this, &MainWindow::updateStatusPill);

    const QByteArray geometry = m_ctx.settings->mainWindowGeometry();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        if (const QScreen* screen = this->screen()) {
            const QRect available = screen->availableGeometry();
            resize(qMin(1180, available.width() - 80), qMin(780, available.height() - 80));
            move(available.center() - rect().center());
        }
    }
    showPage(Page::Translate);
    updateStatusPill();
}

QWidget* MainWindow::buildSidebar()
{
    auto* sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(230);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 26, 0, 20);
    layout->setSpacing(2);

    auto* brand = new QVBoxLayout;
    brand->setContentsMargins(22, 0, 22, 26);
    brand->setSpacing(0);
    QLabel* mark = ui::label(QStringLiteral("HYPERS"), "brandMark");
    QFont markFont = mark->font();
    markFont.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    mark->setFont(markFont);
    QLabel* sub = ui::label(QStringLiteral("TRANSLATOR"), "brandSub");
    QFont subFont = sub->font();
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 5);
    sub->setFont(subFont);
    brand->addWidget(mark);
    brand->addWidget(sub);
    layout->addLayout(brand);

    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    const QList<QPair<QString, Page>> entries = {
        {QStringLiteral("Translate"), Page::Translate},
        {QStringLiteral("History"), Page::History},
        {QStringLiteral("Settings"), Page::Settings},
        {QStringLiteral("About"), Page::About},
    };
    for (const auto& [text, page] : entries) {
        QPushButton* button = ui::button(text);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        group->addButton(button, static_cast<int>(page));
        layout->addWidget(button);
        m_navButtons.append(button);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { showPage(static_cast<Page>(id)); });

    layout->addStretch(1);

    auto* footer = new QVBoxLayout;
    footer->setContentsMargins(18, 0, 18, 0);
    footer->setSpacing(8);
    m_statusPill = ui::label(QString(), "statusPill");
    m_statusPill->setWordWrap(true);
    m_providerLabel = ui::label(QString(), "hint");
    footer->addWidget(m_statusPill);
    footer->addWidget(m_providerLabel);
    layout->addLayout(footer);
    return sidebar;
}

QWidget* MainWindow::buildAboutPage()
{
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(18);
    root->addWidget(ui::label(QStringLiteral("About"), "pageTitle"));
    root->addWidget(ui::label(QStringLiteral("HYPERS TRANSLATOR %1").arg(QStringLiteral(HYPERS_VERSION_STRING)),
                              "pageSubtitle"));

    QVBoxLayout* how = nullptr;
    root->addWidget(ui::card(QStringLiteral("How it works"), &how));
    const QStringList lines = {
        QStringLiteral("• Selected text: the shortcut copies your selection, translates it with your provider and "
                       "pastes the result over it. Your clipboard is restored afterwards."),
        QStringLiteral("• Screen OCR: drag a region; Windows' built-in OCR reads it on this PC (offline), then only "
                       "the recognised text is sent for translation."),
        QStringLiteral("• Providers: DeepL API and Google Cloud Translation. Keys live in Windows Credential Manager."),
        QStringLiteral("• History is stored locally in SQLite and can be turned off or cleared at any time."),
        QStringLiteral("• Translations are produced by the provider you choose (DeepL or Google). Their wording and "
                       "accuracy, and service errors such as an invalid key, a used-up quota or an outage, come from "
                       "that provider's API - not from HYPERS TRANSLATOR."),
    };
    for (const QString& line : lines) {
        QLabel* l = ui::label(line, "muted");
        l->setWordWrap(true);
        how->addWidget(l);
    }

    QVBoxLayout* limits = nullptr;
    root->addWidget(ui::card(QStringLiteral("Windows limitations"), &limits));
    const QStringList limitLines = {
        QStringLiteral("• Apps running as administrator can't receive keystrokes from a non-elevated app (UIPI). "
                       "Run HYPERS TRANSLATOR as administrator to translate inside them."),
        QStringLiteral("• Windows doesn't deliver hotkeys or allow input on secure screens (UAC prompts, lock "
                       "screen, Ctrl+Alt+Del)."),
        QStringLiteral("• Windows OCR uses installed language packs and one language per capture. Content protected "
                       "from capture (DRM video, some password managers) appears black."),
        QStringLiteral("• A shortcut already registered by another app or by Windows can't be taken over."),
    };
    for (const QString& line : limitLines) {
        QLabel* l = ui::label(line, "muted");
        l->setWordWrap(true);
        limits->addWidget(l);
    }
    root->addStretch(1);
    return page;
}

void MainWindow::showPage(Page page)
{
    const int index = static_cast<int>(page);
    m_stack->setCurrentIndex(index);
    if (index < m_navButtons.size())
        m_navButtons.at(index)->setChecked(true);
    if (page == Page::Translate)
        m_translatePage->focusInput();
}

void MainWindow::present()
{
    if (isMinimized())
        showNormal();
    winutil::bringToForeground(this);
}

void MainWindow::setHotkeysPaused(bool paused)
{
    m_paused = paused;
    updateStatusPill();
}

void MainWindow::updateStatusPill()
{
    m_statusPill->setText(m_paused ? QStringLiteral("●  Hotkeys paused") : QStringLiteral("●  Hotkeys active"));
    m_statusPill->setProperty("state", m_paused ? QStringLiteral("paused") : QStringLiteral("active"));
    ui::repolish(m_statusPill);

    const ITranslationProvider* provider = m_ctx.translation->activeProvider();
    if (provider) {
        m_providerLabel->setText(provider->hasApiKey()
                                     ? QStringLiteral("Provider: %1").arg(provider->displayName())
                                     : QStringLiteral("Provider: %1 (no API key)").arg(provider->displayName()));
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (!m_frameStyled) {
        winutil::applyDarkWindowFrame(this, theme::background(), theme::accent().darker(160));
        m_frameStyled = true;
    }
    updateStatusPill();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_ctx.settings->setMainWindowGeometry(saveGeometry());
    if (m_quitting) {
        event->accept();
        return;
    }
    if (m_ctx.settings->closeToTray()) {
        hide();
        event->ignore();
        Q_EMIT hiddenToTray();
        return;
    }
    event->accept();
    Q_EMIT quitRequested();
}

} // namespace hypers
