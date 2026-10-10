#pragma once

#include "platform/ISecretStore.h"

#include <string>

namespace hypers {

// ISecretStore backed by the Windows Credential Manager (generic credentials,
// visible under Control Panel › Credential Manager › Windows Credentials as
// "HYPERS_TRANSLATOR/<name>"). Blobs are encrypted by Windows with DPAPI for
// the current user; no plaintext key ever touches the registry or disk.
class WinCredentialStore final : public ISecretStore {
public:
    explicit WinCredentialStore(QString prefix = QStringLiteral("HYPERS_TRANSLATOR/"));

    bool write(const QString& name, const QString& secret, QString* error = nullptr) override;
    std::optional<QString> read(const QString& name, QString* error = nullptr) const override;
    bool remove(const QString& name, QString* error = nullptr) override;

private:
    std::wstring targetName(const QString& name) const;
    QString m_prefix;
};

} // namespace hypers
