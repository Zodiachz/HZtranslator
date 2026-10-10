#include "core/SingleInstance.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>

#include <windows.h>

Q_LOGGING_CATEGORY(lcInstance, "hypers.instance")

namespace hypers {

namespace {
const QByteArray kActivateCommand = QByteArrayLiteral("activate\n");
}

SingleInstance::SingleInstance(const QString& id, QObject* parent)
    : QObject(parent)
    , m_id(id)
{
}

SingleInstance::~SingleInstance()
{
    if (m_server)
        m_server->close();
    if (m_mutex)
        ::CloseHandle(static_cast<HANDLE>(m_mutex));
}

bool SingleInstance::acquire()
{
    // "Local\" scopes the mutex to the current logon session, so two users on
    // the same machine (fast user switching / RDP) each get their own instance.
    const std::wstring mutexName = L"Local\\" + m_id.toStdWString() + L".Singleton";
    HANDLE mutex = ::CreateMutexW(nullptr, TRUE, mutexName.c_str());
    const DWORD err = ::GetLastError();
    if (!mutex) {
        qCWarning(lcInstance) << "CreateMutexW failed" << err << "- continuing without single-instance guard";
        return true;
    }
    if (err == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(mutex);
        return false;
    }
    m_mutex = mutex;

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(m_id);
    if (!m_server->listen(m_id)) {
        qCWarning(lcInstance) << "Activation pipe unavailable:" << m_server->errorString();
        return true;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (socket->readAll().startsWith(kActivateCommand.trimmed()))
                    Q_EMIT activationRequested();
                socket->disconnectFromServer();
            });
        }
    });
    return true;
}

bool SingleInstance::notifyPrimary() const
{
    QLocalSocket socket;
    socket.connectToServer(m_id);
    if (!socket.waitForConnected(1000))
        return false;
    socket.write(kActivateCommand);
    socket.waitForBytesWritten(1000);
    socket.waitForDisconnected(500);
    return true;
}

} // namespace hypers
