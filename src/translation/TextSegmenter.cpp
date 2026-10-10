#include "translation/TextSegmenter.h"

#include <QStringView>

namespace hypers {

namespace {

bool isSentenceEnd(QChar c)
{
    switch (c.unicode()) {
    case '.': case '!': case '?': case ';':
    case 0x3002: // 。
    case 0xFF01: // ！
    case 0xFF1F: // ？
    case 0x061F: // ؟ Arabic question mark
    case 0x06D4: // ۔ Urdu full stop
        return true;
    default:
        return false;
    }
}

// Best cut position (exclusive end) inside window [0, size).
qsizetype findCut(QStringView window)
{
    const qsizetype size = window.size();
    const qsizetype minimum = size / 3; // don't produce tiny chunks

    const qsizetype paragraph = window.lastIndexOf(u"\n\n");
    if (paragraph >= minimum)
        return paragraph + 2;

    const qsizetype line = window.lastIndexOf(u'\n');
    if (line >= minimum)
        return line + 1;

    for (qsizetype i = size - 1; i >= minimum; --i) {
        if (isSentenceEnd(window.at(i)) && (i + 1 >= size || window.at(i + 1).isSpace()))
            return i + 1;
    }
    for (qsizetype i = size - 1; i >= minimum; --i) {
        if (window.at(i).isSpace())
            return i + 1;
    }
    // Hard cut, but never between the halves of a surrogate pair.
    qsizetype cut = size;
    if (cut > 1 && window.at(cut - 1).isHighSurrogate())
        --cut;
    return cut;
}

TextSegment makeSegment(const QString& piece)
{
    TextSegment seg;
    qsizetype begin = 0;
    qsizetype end = piece.size();
    while (begin < end && piece.at(begin).isSpace())
        ++begin;
    while (end > begin && piece.at(end - 1).isSpace())
        --end;
    seg.leading = piece.left(begin);
    seg.core = piece.mid(begin, end - begin);
    seg.trailing = piece.mid(end);
    return seg;
}

} // namespace

QList<TextSegment> TextSegmenter::split(const QString& text, int maxChars)
{
    QList<TextSegment> segments;
    if (text.isEmpty())
        return segments;
    maxChars = qMax(16, maxChars);

    qsizetype pos = 0;
    const qsizetype n = text.size();
    while (pos < n) {
        if (n - pos <= maxChars) {
            segments.append(makeSegment(text.mid(pos)));
            break;
        }
        const qsizetype cut = findCut(QStringView(text).mid(pos, maxChars));
        segments.append(makeSegment(text.mid(pos, cut)));
        pos += cut;
    }
    return segments;
}

QString TextSegmenter::join(const QList<TextSegment>& segments, const QStringList& translatedCores)
{
    QString out;
    for (qsizetype i = 0; i < segments.size(); ++i) {
        const TextSegment& seg = segments.at(i);
        out += seg.leading;
        if (!seg.core.isEmpty() && i < translatedCores.size())
            out += translatedCores.at(i);
        out += seg.trailing;
    }
    return out;
}

} // namespace hypers
