#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace hypers {

// Ensures one running instance per Windows user session. A second launch asks
// the first instance to show its window (via a named pipe) and exits.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString& id, QObject* parent = nullptr);
    ~SingleInstance() override;

    // True if this process is the primary instance.
    bool acquire();
    // Called by a secondary instance: asks the primary to activate itself.
    bool notifyPrimary() const;

Q_SIGNALS:
    void activationRequested();

private:
    QString m_id;
    void* m_mutex = nullptr; // HANDLE
    QLocalServer* m_server = nullptr;
};

} // namespace hypers
