#pragma once

#include <QImage>
#include <QList>
#include <QString>

namespace hypers {

struct OcrLanguage {
    QString tag;         // BCP-47, e.g. "en-US"
    QString displayName; // e.g. "English (United States)"
};

struct OcrOutput {
    bool ok = false;
    QString text;
    QString languageTag; // recognizer language actually used
    QString error;
};

// Windows.Media.Ocr (the OCR engine built into Windows 10/11) via C++/WinRT.
// Runs fully offline. Recognition uses the OCR language packs installed in
// Windows; each call recognises a single script/language.
class WindowsOcrEngine {
public:
    // Blocking - call from a worker thread (it initialises the thread as MTA).
    // `languageTag` empty = first usable language from the user's Windows profile.
    static OcrOutput recognize(const QImage& image, const QString& languageTag, bool mergeLines);

    // Recognizer languages installed on this PC.
    static QList<OcrLanguage> availableLanguages();

    // Text explaining how to add an OCR language pack.
    static QString installHint(const QString& languageTag = QString());
};

} // namespace hypers
