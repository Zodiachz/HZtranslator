#include "platform/win/AutoStart.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#include <cwchar>
#include <optional>
#include <string>

#include <windows.h>

namespace hypers::autostart {

namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kApprovedKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
constexpr wchar_t kValueName[] = L"HYPERS TRANSLATOR";

std::wstring commandLine()
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString command = QLatin1Char('"') + exe + QStringLiteral("\" ") + QLatin1String(kLaunchArgument);
    return command.toStdWString();
}

std::optional<std::wstring> readRunValue()
{
    DWORD bytes = 0;
    if (::RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
        return std::nullopt;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (::RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value.data(), &bytes)
        != ERROR_SUCCESS)
        return std::nullopt;
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

// Task Manager stores a 12-byte blob; an odd first byte means "disabled".
bool disabledInTaskManager()
{
    BYTE data[16] = {};
    DWORD bytes = sizeof(data);
    if (::RegGetValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName, RRF_RT_REG_BINARY, nullptr, data, &bytes)
        != ERROR_SUCCESS)
        return false;
    return bytes > 0 && (data[0] & 0x01) != 0;
}

QString errorText(LSTATUS status)
{
    return QStringLiteral("Registry error %1").arg(status);
}

} // namespace

bool isEnabled()
{
    return readRunValue().has_value() && !disabledInTaskManager();
}

bool setEnabled(bool enabled, QString* error)
{
    if (enabled) {
        const std::wstring command = commandLine();
        const LSTATUS status = ::RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command.c_str(),
                                                 static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        if (status != ERROR_SUCCESS) {
            if (error)
                *error = errorText(status);
            return false;
        }
        // The user explicitly asked for autostart: clear a previous "disabled"
        // flag set in Task Manager, otherwise Windows would silently skip us.
        ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kApprovedKey, kValueName);
        return true;
    }

    const LSTATUS status = ::RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
        if (error)
            *error = errorText(status);
        return false;
    }
    return true;
}

bool isRunningFromTemporaryLocation()
{
    const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    const QString temp = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return !temp.isEmpty() && appDir.startsWith(temp, Qt::CaseInsensitive);
}

void refreshPathIfEnabled()
{
    const std::optional<std::wstring> current = readRunValue();
    if (current && *current != commandLine()) {
        const std::wstring command = commandLine();
        ::RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command.c_str(),
                          static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
}

} // namespace hypers::autostart
