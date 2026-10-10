#include "core/HistoryStore.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>
#include <QVariant>

Q_LOGGING_CATEGORY(lcHistory, "hypers.history")

namespace hypers {

namespace {

QString escapeLike(QString text)
{
    text.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    text.replace(QLatin1Char('%'), QLatin1String("\\%"));
    text.replace(QLatin1Char('_'), QLatin1String("\\_"));
    return text;
}

} // namespace

HistoryStore::HistoryStore(QObject* parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("hypers_history_") + QUuid::createUuid().toString(QUuid::Id128))
{
}

HistoryStore::~HistoryStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QString HistoryStore::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/history.sqlite");
}

bool HistoryStore::open(const QString& filePath, QString* error)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        qCWarning(lcHistory) << "Cannot open history database:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    const bool created = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS history ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " created_at INTEGER NOT NULL,"
        " origin TEXT NOT NULL,"
        " application TEXT,"
        " source_text TEXT NOT NULL,"
        " translated_text TEXT NOT NULL,"
        " source_lang TEXT,"
        " target_lang TEXT,"
        " provider TEXT)"));
    if (!created) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_history_created ON history(created_at DESC)"));
    m_open = true;
    return true;
}

bool HistoryStore::isOpen() const { return m_open; }

void HistoryStore::setMaxEntries(int maxEntries)
{
    m_maxEntries = qMax(1, maxEntries);
    prune();
}

qint64 HistoryStore::add(HistoryEntry entry)
{
    if (!m_open)
        return 0;
    if (!entry.timestamp.isValid())
        entry.timestamp = QDateTime::currentDateTimeUtc();

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "INSERT INTO history (created_at, origin, application, source_text, translated_text,"
        " source_lang, target_lang, provider) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(entry.timestamp.toMSecsSinceEpoch());
    q.addBindValue(entry.origin);
    q.addBindValue(entry.application);
    q.addBindValue(entry.sourceText);
    q.addBindValue(entry.translatedText);
    q.addBindValue(entry.sourceLanguage);
    q.addBindValue(entry.targetLanguage);
    q.addBindValue(entry.provider);
    if (!q.exec()) {
        qCWarning(lcHistory) << "Insert failed:" << q.lastError().text();
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    prune();
    Q_EMIT changed();
    return id;
}

QList<HistoryEntry> HistoryStore::query(const QString& search, int limit, int offset) const
{
    QList<HistoryEntry> result;
    if (!m_open)
        return result;

    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    QString sql = QStringLiteral(
        "SELECT id, created_at, origin, application, source_text, translated_text, source_lang,"
        " target_lang, provider FROM history");
    const QString needle = search.trimmed();
    if (!needle.isEmpty())
        sql += QStringLiteral(" WHERE source_text LIKE ? ESCAPE '\\' OR translated_text LIKE ? ESCAPE '\\'");
    sql += QStringLiteral(" ORDER BY created_at DESC, id DESC LIMIT ? OFFSET ?");
    q.prepare(sql);
    if (!needle.isEmpty()) {
        const QString pattern = QLatin1Char('%') + escapeLike(needle) + QLatin1Char('%');
        q.addBindValue(pattern);
        q.addBindValue(pattern);
    }
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (!q.exec()) {
        qCWarning(lcHistory) << "Query failed:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        HistoryEntry e;
        e.id = q.value(0).toLongLong();
        e.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(1).toLongLong(), QTimeZone::utc());
        e.origin = q.value(2).toString();
        e.application = q.value(3).toString();
        e.sourceText = q.value(4).toString();
        e.translatedText = q.value(5).toString();
        e.sourceLanguage = q.value(6).toString();
        e.targetLanguage = q.value(7).toString();
        e.provider = q.value(8).toString();
        result.append(e);
    }
    return result;
}

int HistoryStore::count() const
{
    if (!m_open)
        return 0;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM history")) && q.next())
        return q.value(0).toInt();
    return 0;
}

bool HistoryStore::remove(qint64 id)
{
    if (!m_open)
        return false;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("DELETE FROM history WHERE id = ?"));
    q.addBindValue(id);
    const bool ok = q.exec();
    if (ok)
        Q_EMIT changed();
    return ok;
}

bool HistoryStore::clear()
{
    if (!m_open)
        return false;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    const bool ok = q.exec(QStringLiteral("DELETE FROM history"));
    if (ok) {
        q.exec(QStringLiteral("VACUUM"));
        Q_EMIT changed();
    }
    return ok;
}

void HistoryStore::prune()
{
    if (!m_open)
        return;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "DELETE FROM history WHERE id NOT IN"
        " (SELECT id FROM history ORDER BY created_at DESC, id DESC LIMIT ?)"));
    q.addBindValue(m_maxEntries);
    if (!q.exec())
        qCWarning(lcHistory) << "Prune failed:" << q.lastError().text();
}

} // namespace hypers
