#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace hypers {

// A chunk of the input text with its surrounding whitespace split off.
// Providers tend to trim whitespace; keeping it aside guarantees that e.g. a
// selection ending in "\n" is replaced by a translation ending in "\n".
struct TextSegment {
    QString leading;
    QString core;
    QString trailing;
};

class TextSegmenter {
public:
    // Splits `text` into segments whose core is at most `maxChars` long,
    // preferring paragraph, line, sentence and word boundaries (in that order).
    static QList<TextSegment> split(const QString& text, int maxChars);

    // Rebuilds the text; `translatedCores` holds one entry per segment
    // (entries for whitespace-only segments are ignored).
    static QString join(const QList<TextSegment>& segments, const QStringList& translatedCores);
};

} // namespace hypers
