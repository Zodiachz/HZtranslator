#pragma once

#include <initializer_list>

namespace hypers::input {

// Marker in INPUT::dwExtraInfo so injected events can be told apart in logs/hooks.
constexpr unsigned long long kInjectedMarker = 0x48595052; // "HYPR"

bool isKeyDown(unsigned int virtualKey);

// True while Ctrl, Alt, Shift or a Win key is held (physically or logically).
bool anyModifierDown();

// True while any of `virtualKeys` or a modifier is held.
bool anyKeyDown(std::initializer_list<unsigned int> virtualKeys);

// Sends key-up events for every modifier that is still logically down.
// Last resort when the user keeps holding the hotkey's modifiers.
void releaseModifiers();

// Sends Ctrl+<key> as a complete press/release sequence. Returns false if
// SendInput rejected the events (e.g. input blocked by another thread).
// Note: UIPI blocking (target app elevated) is silent - SendInput still
// reports success - so callers check the target's integrity level first.
bool sendCtrlChord(unsigned int virtualKey);

} // namespace hypers::input
