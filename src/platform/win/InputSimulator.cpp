#include "platform/win/InputSimulator.h"

#include <array>
#include <vector>

#include <windows.h>

namespace hypers::input {

namespace {

constexpr std::array<WORD, 8> kModifierKeys = {
    VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN,
};

bool isExtendedKey(WORD vk)
{
    switch (vk) {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        return true;
    default:
        return false;
    }
}

INPUT keyEvent(WORD vk, bool down)
{
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.wScan = static_cast<WORD>(::MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (isExtendedKey(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    input.ki.dwExtraInfo = static_cast<ULONG_PTR>(kInjectedMarker);
    return input;
}

bool send(std::vector<INPUT>& inputs)
{
    if (inputs.empty())
        return true;
    const UINT sent = ::SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent == inputs.size();
}

} // namespace

bool isKeyDown(unsigned int virtualKey)
{
    return (::GetAsyncKeyState(static_cast<int>(virtualKey)) & 0x8000) != 0;
}

bool anyModifierDown()
{
    for (WORD vk : kModifierKeys) {
        if (isKeyDown(vk))
            return true;
    }
    return false;
}

bool anyKeyDown(std::initializer_list<unsigned int> virtualKeys)
{
    if (anyModifierDown())
        return true;
    for (unsigned int vk : virtualKeys) {
        if (vk && isKeyDown(vk))
            return true;
    }
    return false;
}

void releaseModifiers()
{
    std::vector<INPUT> inputs;
    for (WORD vk : kModifierKeys) {
        if (isKeyDown(vk))
            inputs.push_back(keyEvent(vk, false));
    }
    send(inputs);
}

bool sendCtrlChord(unsigned int virtualKey)
{
    const WORD key = static_cast<WORD>(virtualKey);
    std::vector<INPUT> inputs = {
        keyEvent(VK_LCONTROL, true),
        keyEvent(key, true),
        keyEvent(key, false),
        keyEvent(VK_LCONTROL, false),
    };
    return send(inputs);
}

} // namespace hypers::input
