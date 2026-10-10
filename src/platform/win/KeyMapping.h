#pragma once

#include <QKeySequence>
#include <QString>

#include <optional>

namespace hypers {

// A hotkey in RegisterHotKey() terms.
struct NativeHotkey {
    unsigned int modifiers = 0; // MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN
    unsigned int virtualKey = 0;

    bool operator==(const NativeHotkey&) const = default;
};

// Converts the first key combination of `sequence` to a native hotkey.
// Returns std::nullopt (and sets *error) for sequences that can't or shouldn't
// be registered globally - e.g. plain letters without a modifier.
std::optional<NativeHotkey> toNativeHotkey(const QKeySequence& sequence, QString* error = nullptr);

// Qt::Key -> Windows virtual-key code, 0 if unmapped.
unsigned int qtKeyToVirtualKey(int qtKey);

} // namespace hypers
