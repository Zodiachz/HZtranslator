#pragma once

#include <QString>

#include <optional>

namespace hypers {

// Storage for secrets such as API keys. The Windows implementation uses the
// Credential Manager (DPAPI-encrypted, bound to the user's logon credentials).
class ISecretStore {
public:
    virtual ~ISecretStore() = default;

    virtual bool write(const QString& name, const QString& secret, QString* error = nullptr) = 0;
    // std::nullopt if absent or unreadable (error set only for real failures).
    virtual std::optional<QString> read(const QString& name, QString* error = nullptr) const = 0;
    virtual bool remove(const QString& name, QString* error = nullptr) = 0;
};

// Credential name under which a provider's API key is stored.
inline QString apiKeySecretName(const QString& providerId)
{
    return providerId + QStringLiteral("/api_key");
}

} // namespace hypers
