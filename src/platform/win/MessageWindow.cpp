#include "platform/win/MessageWindow.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMsgWindow, "hypers.win")

namespace hypers {

namespace {
constexpr wchar_t kClassName[] = L"HypersTranslator.MessageWindow";
} // namespace

MessageWindow::MessageWindow(Handler handler)
    : m_handler(std::move(handler))
{
    static const bool classRegistered = [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &MessageWindow::windowProc;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.lpszClassName = kClassName;
        if (!::RegisterClassExW(&wc) && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            qCCritical(lcMsgWindow) << "RegisterClassExW failed:" << ::GetLastError();
            return false;
        }
        return true;
    }();
    if (!classRegistered)
        return;

    // `this` travels through lpCreateParams so messages sent during creation are routed too.
    m_hwnd = ::CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                               ::GetModuleHandleW(nullptr), this);
    if (!m_hwnd)
        qCCritical(lcMsgWindow) << "CreateWindowExW(HWND_MESSAGE) failed:" << ::GetLastError();
}

MessageWindow::~MessageWindow()
{
    if (m_hwnd) {
        ::SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
        ::DestroyWindow(m_hwnd);
    }
}

LRESULT CALLBACK MessageWindow::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<MessageWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->m_handler) {
        LRESULT result = 0;
        if (self->m_handler(message, wParam, lParam, &result))
            return result;
    }
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace hypers
