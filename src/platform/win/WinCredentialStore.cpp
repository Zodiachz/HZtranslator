#include "platform/win/WinCredentialStore.h"

#include <QByteArray>

#include <string>

#include <windows.h>
#include <wincred.h>

namespace hypers {

namespace {

QString lastErrorText(DWORD code)
{
    wchar_t* buffer = nullptr;
    const DWORD len = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
        0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    QString text = len ? QString::fromWCharArray(buffer, static_cast<int>(len)).trimmed()
                       : QStringLiteral("Windows error %1").arg(code);
    if (buffer)
        ::LocalFree(buffer);
    return text;
}

} // namespace

WinCredentialStore::WinCredentialStore(QString prefix)
    : m_prefix(std::move(prefix))
{
}

std::wstring WinCredentialStore::targetName(const QString& name) const
{
    const QString target = m_prefix + name;
    return target.toStdWString();
}

bool WinCredentialStore::write(const QString& name, const QString& secret, QString* error)
{
    QByteArray blob = secret.toUtf8();
    if (blob.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        if (error)
            *error = QStringLiteral("Secret is too long for the Windows Credential Manager.");
        return false;
    }

    const std::wstring target = targetName(name);
    std::wstring user = L"HYPERS TRANSLATOR";

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    // LOCAL_MACHINE persistence = survives logoff, stays on this machine, still per-user.
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = user.data();

    const BOOL ok = ::CredWriteW(&credential, 0);
    const DWORD err = ::GetLastError();
    ::SecureZeroMemory(blob.data(), static_cast<SIZE_T>(blob.size()));
    if (!ok) {
        if (error)
            *error = lastErrorText(err);
        return false;
    }
    return true;
}

std::optional<QString> WinCredentialStore::read(const QString& name, QString* error) const
{
    const std::wstring target = targetName(name);
    PCREDENTIALW credential = nullptr;
    if (!::CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        const DWORD err = ::GetLastError();
        if (err != ERROR_NOT_FOUND && error)
            *error = lastErrorText(err);
        return std::nullopt;
    }

    QString secret = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob),
                                       static_cast<qsizetype>(credential->CredentialBlobSize));
    if (credential->CredentialBlob && credential->CredentialBlobSize)
        ::SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
    ::CredFree(credential);
    return secret;
}

bool WinCredentialStore::remove(const QString& name, QString* error)
{
    const std::wstring target = targetName(name);
    if (!::CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_NOT_FOUND)
            return true;
        if (error)
            *error = lastErrorText(err);
        return false;
    }
    return true;
}

} // namespace hypers
