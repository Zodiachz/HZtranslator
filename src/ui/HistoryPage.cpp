#include "ui/HistoryPage.h"

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "core/Languages.h"
#include "ui/UiHelpers.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableView>

namespace hypers {

namespace {
constexpr int kDisplayLimit = 2000;

QString singleLine(const QString& text, int maxChars = 220)
{
    QString s = text.simplified();
    if (s.size() > maxChars)
        s = s.left(maxChars - 1) + QChar(0x2026);
    return s;
}
} // namespace

class HistoryModel : public QAbstractTableModel {
public:
    enum Column { Time, Origin, Original, Translation, Languages, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;

    void setEntries(QList<HistoryEntry> entries)
    {
        beginResetModel();
        m_entries = std::move(entries);
        endResetModel();
    }

    const HistoryEntry* entry(int row) const
    {
        return (row >= 0 && row < m_entries.size()) ? &m_entries.at(row) : nullptr;
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        switch (section) {
        case Time: return QStringLiteral("When");
        case Origin: return QStringLiteral("Source");
        case Original: return QStringLiteral("Original");
        case Translation: return QStringLiteral("Translation");
        case Languages: return QStringLiteral("Languages");
        default: return {};
        }
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        const HistoryEntry* e = entry(index.row());
        if (!e)
            return {};
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case Time: {
                const QDateTime local = e->timestamp.toLocalTime();
                return local.date() == QDate::currentDate()
                    ? local.toString(QStringLiteral("HH:mm"))
                    : QLocale().toString(local, QLocale::ShortFormat);
            }
            case Origin: {
                QString origin = e->origin == QLatin1String("ocr")         ? QStringLiteral("Screen OCR")
                    : e->origin == QLatin1String("selection")              ? QStringLiteral("Selection")
                    : e->origin == QLatin1String("typed")                  ? QStringLiteral("Typed text")
                                                                           : QStringLiteral("Manual");
                if (!e->application.isEmpty())
                    origin += QStringLiteral(" · ") + e->application;
                return origin;
            }
            case Original: return singleLine(e->sourceText);
            case Translation: return singleLine(e->translatedText);
            case Languages: {
                const QString source = hypers::Languages::nameForProviderCode(e->sourceLanguage);
                const LanguageInfo* target = hypers::Languages::find(e->targetLanguage);
                return QString((source.isEmpty() ? QStringLiteral("?") : source) + QStringLiteral(" → ")
                               + (target ? target->englishName : e->targetLanguage));
            }
            default: return {};
            }
        }
        if (role == Qt::ToolTipRole) {
            if (index.column() == Original)
                return e->sourceText.left(2000);
            if (index.column() == Translation)
                return e->translatedText.left(2000);
        }
        if (role == Qt::ForegroundRole && (index.column() == Time || index.column() == Origin))
            return QColor(0x9A, 0x9A, 0xA8);
        return {};
    }

private:
    QList<HistoryEntry> m_entries;
};

HistoryPage::HistoryPage(const AppContext& context, QWidget* parent)
    : QWidget(parent)
    , m_ctx(context)
    , m_model(new HistoryModel(this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(32, 28, 32, 28);
    root->setSpacing(18);

    auto* header = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(4);
    titles->addWidget(ui::label(QStringLiteral("History"), "pageTitle"));
    titles->addWidget(ui::label(QStringLiteral("Stored only on this PC. Double-click an entry to open it in the translator."),
                                "pageSubtitle"));
    header->addLayout(titles, 1);
    m_search = new QLineEdit;
    m_search->setPlaceholderText(QStringLiteral("Search history…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(280);
    header->addWidget(m_search, 0, Qt::AlignBottom);
    root->addLayout(header);

    m_table = new QTableView;
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(38);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(HistoryModel::Time, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(HistoryModel::Origin, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(HistoryModel::Original, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(HistoryModel::Translation, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(HistoryModel::Languages, QHeaderView::ResizeToContents);
    root->addWidget(m_table, 1);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_count = ui::label(QString(), "hint");
    m_copyTranslation = ui::button(QStringLiteral("Copy translation"), "accent");
    m_copyOriginal = ui::button(QStringLiteral("Copy original"));
    m_open = ui::button(QStringLiteral("Open in translator"));
    m_delete = ui::button(QStringLiteral("Delete"), "ghost");
    m_clear = ui::button(QStringLiteral("Clear all…"), "danger");
    actions->addWidget(m_count, 1);
    actions->addWidget(m_clear);
    actions->addWidget(m_delete);
    actions->addWidget(m_open);
    actions->addWidget(m_copyOriginal);
    actions->addWidget(m_copyTranslation);
    root->addLayout(actions);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(250);
    connect(&m_searchDebounce, &QTimer::timeout, this, &HistoryPage::reload);
    connect(m_search, &QLineEdit::textChanged, &m_searchDebounce, qOverload<>(&QTimer::start));

    connect(m_ctx.history, &HistoryStore::changed, this, [this] {
        if (isVisible())
            reload();
        else
            m_dirty = true;
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &HistoryPage::updateButtons);
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (const HistoryEntry* e = m_model->entry(index.row()))
            Q_EMIT openInTranslator(e->sourceText);
    });
    connect(m_copyTranslation, &QPushButton::clicked, this, [this] {
        if (const HistoryEntry* e = m_model->entry(selectedRow()))
            QGuiApplication::clipboard()->setText(e->translatedText);
    });
    connect(m_copyOriginal, &QPushButton::clicked, this, [this] {
        if (const HistoryEntry* e = m_model->entry(selectedRow()))
            QGuiApplication::clipboard()->setText(e->sourceText);
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (const HistoryEntry* e = m_model->entry(selectedRow()))
            Q_EMIT openInTranslator(e->sourceText);
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const qint64 id = selectedId();
        if (id > 0)
            m_ctx.history->remove(id);
    });
    connect(m_clear, &QPushButton::clicked, this, [this] {
        const auto answer = QMessageBox::question(this, QStringLiteral("Clear history"),
                                                  QStringLiteral("Delete all translation history from this PC?"),
                                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer == QMessageBox::Yes)
            m_ctx.history->clear();
    });

    updateButtons();
}

void HistoryPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_dirty)
        reload();
}

void HistoryPage::reload()
{
    m_dirty = false;
    const qint64 previous = selectedId();
    m_model->setEntries(m_ctx.history->query(m_search->text(), kDisplayLimit));
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (m_model->entry(row)->id == previous) {
            m_table->selectRow(row);
            break;
        }
    }
    const int total = m_ctx.history->count();
    m_count->setText(m_search->text().trimmed().isEmpty()
                         ? QStringLiteral("%1 entries").arg(total)
                         : QStringLiteral("%1 matches · %2 entries").arg(m_model->rowCount()).arg(total));
    if (!m_ctx.settings->historyEnabled())
        m_count->setText(m_count->text() + QStringLiteral("  ·  saving is turned off in Settings"));
    updateButtons();
}

int HistoryPage::selectedRow() const
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

qint64 HistoryPage::selectedId() const
{
    const HistoryEntry* e = m_model->entry(selectedRow());
    return e ? e->id : 0;
}

void HistoryPage::updateButtons()
{
    const bool hasSelection = selectedRow() >= 0;
    m_copyTranslation->setEnabled(hasSelection);
    m_copyOriginal->setEnabled(hasSelection);
    m_open->setEnabled(hasSelection);
    m_delete->setEnabled(hasSelection);
    m_clear->setEnabled(m_model->rowCount() > 0 || !m_search->text().isEmpty());
}

} // namespace hypers
