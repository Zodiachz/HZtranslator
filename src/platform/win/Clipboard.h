#pragma once

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

#include <windows.h>

namespace hypers::clipboard {

// Raw Win32 clipboard access. Qt's QClipboard caches data and can't express
// "exclude from clipboard history", so the selection workflow uses this.

// Monotonic counter that changes on every clipboard write (any process).
DWORD sequenceNumber();

// Full copy of every memory-backed clipboard format, so the user's clipboard
// (text, rich text, images, files, ...) can be put back after we borrowed it.
class Snapshot {
public:
    static std::optional<Snapshot> capture(HWND owner, QString* error = nullptr);

    // Restores all captured formats. The restored content is flagged so it is
    // not added a second time to Windows clipboard history (Win+V).
    bool restore(HWND owner, QString* error = nullptr) const;
    bool isEmpty() const { return m_items.empty(); }

private:
    struct Item {
        UINT format = 0;
        QByteArray data;
    };
    std::vector<Item> m_items;
};

std::optional<QString> readText(HWND owner);

// Puts plain text on the clipboard. With `transient` the content is flagged
// with ExcludeClipboardContentFromMonitorProcessing / CanIncludeInClipboardHistory=0
// / CanUploadToCloudClipboard=0 so it stays out of Win+V history, cloud
// clipboard sync and clipboard managers.
bool setText(HWND owner, const QString& text, bool transient, QString* error = nullptr);

} // namespace hypers::clipboard
