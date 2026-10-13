#include "core/Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>
#include <QThread>

#include <windows.h>

namespace hypers {

namespace {

constexpr qint64 kMaxLogSize = 2 * 1024 * 1024;

QMutex g_mutex;
QFile* g_file = nullptr;
QString g_path;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO ";
    case QtWarningMsg: return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "?????";
}

void rotateIfNeeded(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || info.size() < kMaxLogSize)
        return;
    const QString backup = path + QStringLiteral(".1");
    QFile::remove(backup);
    QFile::rename(path, backup);
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    QString category;
    if (context.category && qstrcmp(context.category, "default") != 0)
        category = QString::fromLatin1(context.category) + QStringLiteral(": ");

    const QString line = QStringLiteral("%1 [%2] [%3] %4%5\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16),
                                  category,
                                  message);

    ::OutputDebugStringW(reinterpret_cast<const wchar_t*>(line.utf16()));

    {
        QMutexLocker lock(&g_mutex);
        if (g_file && g_file->isOpen()) {
            // The app runs for weeks from the tray: rotate here too, not only at startup.
            if (g_file->size() >= kMaxLogSize) {
                g_file->close();
                rotateIfNeeded(g_path);
                g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
            }
            g_file->write(line.toUtf8());
            g_file->flush();
        }
    }

    if (type == QtFatalMsg)
        ::abort();
}

} // namespace

QString Logger::logFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/logs");
    return dir + QStringLiteral("/hypers.log");
}

void Logger::install()
{
    const QString path = logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    rotateIfNeeded(path);

    {
        QMutexLocker lock(&g_mutex);
        if (!g_file) {
            g_path = path;
            g_file = new QFile(path); // intentionally leaked: must outlive every static destructor that logs
            if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
                ::OutputDebugStringW(L"HYPERS TRANSLATOR: cannot open log file\n");
        }
    }
    qInstallMessageHandler(messageHandler);
}

} // namespace hypers
