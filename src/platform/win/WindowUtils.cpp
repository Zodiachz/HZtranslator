#include "platform/win/WindowUtils.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

#include <iterator>
#include <vector>

#include <dwmapi.h>

namespace hypers::winutil {

namespace {

// DWMWINDOWATTRIBUTE values; spelled out so older SDK headers still compile.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr DWORD kDwmBorderColor = 34;
constexpr DWORD kDwmCaptionColor = 35;
constexpr DWORD kDwmTextColor = 36;
constexpr int kCornerRound = 2;
constexpr int kCornerRoundSmall = 3;

std::optional<DWORD> integrityLevel(HANDLE process, bool* accessDenied)
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(process, TOKEN_QUERY, &token)) {
        if (accessDenied)
            *accessDenied = ::GetLastError() == ERROR_ACCESS_DENIED;
        return std::nullopt;
    }
    DWORD size = 0;
    ::GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    std::optional<DWORD> level;
    if (size && ::GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), size, &size)) {
        const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
        const UCHAR count = *::GetSidSubAuthorityCount(label->Label.Sid);
        if (count > 0)
            level = *::GetSidSubAuthority(label->Label.Sid, count - 1);
    }
    ::CloseHandle(token);
    return level;
}

COLORREF toColorRef(const QColor& c)
{
    return RGB(c.red(), c.green(), c.blue());
}

} // namespace

HWND foregroundWindow()
{
    return ::GetForegroundWindow();
}

QString processNameForWindow(HWND hwnd)
{
    if (!hwnd)
        return {};
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t path[MAX_PATH * 4];
    DWORD length = static_cast<DWORD>(std::size(path));
    QString name;
    if (::QueryFullProcessImageNameW(process, 0, path, &length))
        name = QFileInfo(QString::fromWCharArray(path, static_cast<qsizetype>(length))).fileName();
    ::CloseHandle(process);
    return name;
}

bool isOwnProcessWindow(HWND hwnd)
{
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    return pid == ::GetCurrentProcessId();
}

bool isCurrentProcessElevated()
{
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    const bool elevated =
        ::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
    ::CloseHandle(token);
    return elevated;
}

bool isWindowMoreElevatedThanUs(HWND hwnd)
{
    if (!hwnd)
        return false;
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId())
        return false;

    HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return ::GetLastError() == ERROR_ACCESS_DENIED;

    bool accessDenied = false;
    const std::optional<DWORD> theirs = integrityLevel(process, &accessDenied);
    ::CloseHandle(process);
    const std::optional<DWORD> ours = integrityLevel(::GetCurrentProcess(), nullptr);

    if (!theirs)
        return accessDenied; // can't read its token: it runs above us
    return ours && *theirs > *ours;
}

std::optional<QRect> caretRectForWindow(HWND hwnd)
{
    if (!hwnd)
        return std::nullopt;
    const DWORD thread = ::GetWindowThreadProcessId(hwnd, nullptr);
    GUITHREADINFO info{};
    info.cbSize = sizeof(info);
    if (!::GetGUIThreadInfo(thread, &info) || !info.hwndCaret)
        return std::nullopt;
    const RECT& r = info.rcCaret;
    if (r.right <= r.left && r.bottom <= r.top)
        return std::nullopt;
    POINT topLeft{r.left, r.top};
    POINT bottomRight{r.right, r.bottom};
    ::ClientToScreen(info.hwndCaret, &topLeft);
    ::ClientToScreen(info.hwndCaret, &bottomRight);
    const QPoint a = physicalToLogical(QPoint(topLeft.x, topLeft.y));
    const QPoint b = physicalToLogical(QPoint(bottomRight.x, bottomRight.y));
    return QRect(a, b).normalized().adjusted(0, 0, 1, 1);
}

QPoint physicalToLogical(const QPoint& physical)
{
    // Qt keeps each screen's top-left at its native position and scales the
    // rest by the screen's device pixel ratio.
    for (QScreen* screen : QGuiApplication::screens()) {
        const QRect logical = screen->geometry();
        const qreal dpr = screen->devicePixelRatio();
        const QRect native(logical.topLeft(), logical.size() * dpr);
        if (native.contains(physical)) {
            const QPointF offset = QPointF(physical - native.topLeft()) / dpr;
            return logical.topLeft() + offset.toPoint();
        }
    }
    return physical;
}

void applyDarkWindowFrame(QWidget* window, const QColor& caption, const QColor& border)
{
    if (!window)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const BOOL dark = TRUE;
    ::DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    const COLORREF captionColor = toColorRef(caption);
    const COLORREF borderColor = toColorRef(border);
    const COLORREF textColor = RGB(236, 236, 241);
    ::DwmSetWindowAttribute(hwnd, kDwmCaptionColor, &captionColor, sizeof(captionColor));
    ::DwmSetWindowAttribute(hwnd, kDwmBorderColor, &borderColor, sizeof(borderColor));
    ::DwmSetWindowAttribute(hwnd, kDwmTextColor, &textColor, sizeof(textColor));
    const int corner = kCornerRound;
    ::DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner));
}

void applyRoundedCorners(QWidget* window, bool smallRadius)
{
    if (!window)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const int corner = smallRadius ? kCornerRoundSmall : kCornerRound;
    ::DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference, &corner, sizeof(corner));
}

void bringToForeground(QWidget* window)
{
    if (!window)
        return;
    window->show();
    window->raise();
    window->activateWindow();
    ::SetForegroundWindow(reinterpret_cast<HWND>(window->winId()));
}

} // namespace hypers::winutil
