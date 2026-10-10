#include "translation/TextSegmenter.h"

#include <QtTest>

using namespace hypers;

class TestTextSegmenter : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void keepsSurroundingWhitespace()
    {
        const QList<TextSegment> segments = TextSegmenter::split(QStringLiteral("  Hello world\n"), 1000);
        QCOMPARE(segments.size(), 1);
        QCOMPARE(segments.first().leading, QStringLiteral("  "));
        QCOMPARE(segments.first().core, QStringLiteral("Hello world"));
        QCOMPARE(segments.first().trailing, QStringLiteral("\n"));
        QCOMPARE(TextSegmenter::join(segments, {QStringLiteral("Hallo Welt")}), QStringLiteral("  Hallo Welt\n"));
    }

    void whitespaceOnlyHasEmptyCore()
    {
        const QList<TextSegment> segments = TextSegmenter::split(QStringLiteral(" \n\t "), 1000);
        QCOMPARE(segments.size(), 1);
        QVERIFY(segments.first().core.isEmpty());
        QCOMPARE(TextSegmenter::join(segments, {QStringLiteral("ignored")}), QStringLiteral(" \n\t "));
    }

    void splitsLongTextAtBoundaries()
    {
        QString text;
        for (int i = 0; i < 40; ++i)
            text += QStringLiteral("Sentence number %1 is here. ").arg(i);
        text += QStringLiteral("\n\nSecond paragraph follows with more words.");

        const int limit = 200;
        const QList<TextSegment> segments = TextSegmenter::split(text, limit);
        QVERIFY(segments.size() > 1);

        QStringList identity;
        QString rebuilt;
        for (const TextSegment& seg : segments) {
            QVERIFY(seg.core.size() <= limit);
            QVERIFY2(seg.core.endsWith(QLatin1Char('.')), qPrintable(seg.core)); // cut at sentence ends
            identity << seg.core;
            rebuilt += seg.leading + seg.core + seg.trailing;
        }
        QCOMPARE(rebuilt, text); // lossless
        QCOMPARE(TextSegmenter::join(segments, identity), text);
    }

    void neverSplitsSurrogatePairs()
    {
        QString text;
        for (int i = 0; i < 100; ++i)
            text += QString::fromUtf8("😀"); // 2 UTF-16 code units each, no spaces
        const QList<TextSegment> segments = TextSegmenter::split(text, 33);
        for (const TextSegment& seg : segments) {
            QVERIFY(!seg.core.isEmpty());
            QVERIFY(!seg.core.back().isHighSurrogate());
            QVERIFY(!seg.core.front().isLowSurrogate());
        }
    }
};

QTEST_GUILESS_MAIN(TestTextSegmenter)
#include "tst_textsegmenter.moc"
