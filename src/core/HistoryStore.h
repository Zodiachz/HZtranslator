#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

namespace hypers {

struct HistoryEntry {
    qint64 id = 0;
    QDateTime timestamp;
    QString origin;        // "selection", "ocr" or "manual"
    QString application;   // executable of the source app, if known
    QString sourceText;
    QString translatedText;
    QString sourceLanguage; // as detected/requested
    QString targetLanguage;
    QString provider;
};

// Local translation history in SQLite
// (%LOCALAPPDATA%\Hypers\HYPERS TRANSLATOR\history.sqlite).
class HistoryStore : public QObject {
    Q_OBJECT
public:
    explicit HistoryStore(QObject* parent = nullptr);
    ~HistoryStore() override;

    bool open(const QString& filePath, QString* error = nullptr);
    bool isOpen() const;

    void setMaxEntries(int maxEntries);

    qint64 add(HistoryEntry entry);
    QList<HistoryEntry> query(const QString& search, int limit, int offset = 0) const;
    int count() const;
    bool remove(qint64 id);
    bool clear();

    static QString defaultPath();

Q_SIGNALS:
    void changed();

private:
    void prune();

    QString m_connectionName;
    int m_maxEntries = 1000;
    bool m_open = false;
};

} // namespace hypers
