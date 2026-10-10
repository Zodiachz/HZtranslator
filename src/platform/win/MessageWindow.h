#pragma once

#include <functional>

#include <windows.h>

namespace hypers {

// RAII wrapper for a hidden message-only window (HWND_MESSAGE). Used as the
// target for WM_HOTKEY and as the clipboard owner (SetClipboardData requires
// an owner window). Messages are pumped by Qt's event dispatcher, so the
// handler runs on the GUI thread.
class MessageWindow {
public:
    // Return true if the message was handled; *result is then returned to Windows.
    using Handler = std::function<bool(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* result)>;

    explicit MessageWindow(Handler handler = {});
    ~MessageWindow();

    MessageWindow(const MessageWindow&) = delete;
    MessageWindow& operator=(const MessageWindow&) = delete;

    HWND hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    Handler m_handler;
};

} // namespace hypers
