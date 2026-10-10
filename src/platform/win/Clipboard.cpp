#include "platform/win/Clipboard.h"

#include <QLoggingCategory>

#include <cstring>

Q_LOGGING_CATEGORY(lcClipboard, "hypers.clipboard")

namespace hypers::clipboard {

namespace {

constexpr SIZE_T kMaxFormatBytes = 64ull * 1024 * 1024; // skip absurdly large formats

// Another process may hold the clipboard open for a moment (clipboard
// managers, RDP clipboard sync, Office); retry briefly instead of failing.
class ClipboardLock {
public:
    explicit ClipboardLock(HWND owner)
    {
        for (int attempt = 0; attempt < 20; ++attempt) {
            if (::OpenClipboard(owner)) {
                m_open = true;
                return;
            }
            ::Sleep(10);
        }
    }
    ~ClipboardLock()
    {
        if (m_open)
            ::CloseClipboard();
    }
    bool isOpen() const { return m_open; }

private:
    bool m_open = false;
};

bool isGdiOrPrivateHandleFormat(UINT format)
{
    switch (format) {
    case CF_BITMAP:          // GDI handle - CF_DIB/CF_DIBV5 carry the same image as memory
    case CF_METAFILEPICT:
    case CF_PALETTE:
    case CF_ENHMETAFILE:
    case CF_OWNERDISPLAY:
    case CF_DSPBITMAP:
    case CF_DSPMETAFILEPICT:
    case CF_DSPENHMETAFILE:
        return true;
    default:
        break;
    }
    return (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST)
        || (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST);
}

bool setGlobalData(UINT format, const void* data, SIZE_T size)
{
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, size ? size : 1);
    if (!memory)
        return false;
    if (void* target = ::GlobalLock(memory)) {
        if (size)
            std::memcpy(target, data, size);
        ::GlobalUnlock(memory);
    }
    if (!::SetClipboardData(format, memory)) {
        ::GlobalFree(memory); // ownership is transferred only on success
        return false;
    }
    return true;
}

void addTransientMarkers()
{
    static const UINT excludeFormat = ::RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    static const UINT historyFormat = ::RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    static const UINT cloudFormat = ::RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
    const DWORD zero = 0;
    if (excludeFormat)
        setGlobalData(excludeFormat, &zero, sizeof(zero));
    if (historyFormat)
        setGlobalData(historyFormat, &zero, sizeof(zero));
    if (cloudFormat)
        setGlobalData(cloudFormat, &zero, sizeof(zero));
}

} // namespace

DWORD sequenceNumber()
{
    return ::GetClipboardSequenceNumber();
}

std::optional<Snapshot> Snapshot::capture(HWND owner, QString* error)
{
    ClipboardLock lock(owner);
    if (!lock.isOpen()) {
        if (error)
            *error = QStringLiteral("The clipboard is locked by another application.");
        return std::nullopt;
    }

    Snapshot snapshot;
    UINT format = 0;
    while ((format = ::EnumClipboardFormats(format)) != 0) {
        if (isGdiOrPrivateHandleFormat(format))
            continue;
        HANDLE handle = ::GetClipboardData(format);
        if (!handle)
            continue;
        const SIZE_T size = ::GlobalSize(handle);
        if (size == 0 || size > kMaxFormatBytes)
            continue;
        const void* source = ::GlobalLock(handle);
        if (!source)
            continue;
        Item item;
        item.format = format;
        item.data = QByteArray(static_cast<const char*>(source), static_cast<qsizetype>(size));
        ::GlobalUnlock(handle);
        snapshot.m_items.push_back(std::move(item));
    }
    return snapshot;
}

bool Snapshot::restore(HWND owner, QString* error) const
{
    ClipboardLock lock(owner);
    if (!lock.isOpen()) {
        if (error)
            *error = QStringLiteral("The clipboard is locked by another application.");
        return false;
    }
    ::EmptyClipboard();
    int restored = 0;
    for (const Item& item : m_items) {
        if (setGlobalData(item.format, item.data.constData(), static_cast<SIZE_T>(item.data.size())))
            ++restored;
    }
    if (!m_items.empty())
        addTransientMarkers();
    qCDebug(lcClipboard) << "Restored" << restored << "of" << m_items.size() << "clipboard formats";
    return true;
}

std::optional<QString> readText(HWND owner)
{
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
        return std::nullopt;
    ClipboardLock lock(owner);
    if (!lock.isOpen())
        return std::nullopt;
    HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
    if (!handle)
        return std::nullopt;
    const auto* text = static_cast<const wchar_t*>(::GlobalLock(handle));
    if (!text)
        return std::nullopt;
    // GlobalSize may include padding; stop at the terminator.
    const SIZE_T maxChars = ::GlobalSize(handle) / sizeof(wchar_t);
    SIZE_T length = 0;
    while (length < maxChars && text[length] != L'\0')
        ++length;
    QString result = QString::fromWCharArray(text, static_cast<qsizetype>(length));
    ::GlobalUnlock(handle);
    return result;
}

bool setText(HWND owner, const QString& text, bool transient, QString* error)
{
    ClipboardLock lock(owner);
    if (!lock.isOpen()) {
        if (error)
            *error = QStringLiteral("The clipboard is locked by another application.");
        return false;
    }
    ::EmptyClipboard();

    // Normalise to CRLF - the native clipboard convention, expected by e.g. Notepad and Office.
    QString native = text;
    native.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    native.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    const std::wstring wide = native.toStdWString();
    const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
    if (!setGlobalData(CF_UNICODETEXT, wide.c_str(), bytes)) {
        if (error)
            *error = QStringLiteral("SetClipboardData failed (error %1).").arg(::GetLastError());
        return false;
    }
    if (transient)
        addTransientMarkers();
    return true;
}

} // namespace hypers::clipboard
