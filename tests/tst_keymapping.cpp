#include "platform/win/KeyMapping.h"

#include <QtTest>

#include <windows.h>

using namespace hypers;

class TestKeyMapping : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void defaultsMap()
    {
        const auto translate = toNativeHotkey(QKeySequence(QStringLiteral("Ctrl+Alt+T")));
        QVERIFY(translate);
        QCOMPARE(translate->modifiers, UINT(MOD_CONTROL | MOD_ALT));
        QCOMPARE(translate->virtualKey, UINT('T'));

        const auto ocr = toNativeHotkey(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        QVERIFY(ocr);
        QCOMPARE(ocr->modifiers, UINT(MOD_CONTROL | MOD_SHIFT));
        QCOMPARE(ocr->virtualKey, UINT('S'));
    }

    void specialKeys()
    {
        QCOMPARE(qtKeyToVirtualKey(Qt::Key_F13), UINT(VK_F13));
        QCOMPARE(qtKeyToVirtualKey(Qt::Key_Space), UINT(VK_SPACE));
        QCOMPARE(qtKeyToVirtualKey(Qt::Key_PageDown), UINT(VK_NEXT));
        QCOMPARE(qtKeyToVirtualKey(Qt::Key_7), UINT('7'));

        const auto win = toNativeHotkey(QKeySequence(QStringLiteral("Meta+Shift+F2")));
        QVERIFY(win);
        QCOMPARE(win->modifiers, UINT(MOD_WIN | MOD_SHIFT));
    }

    void rejectsUnsafeSequences_data()
    {
        QTest::addColumn<QString>("sequence");
        QTest::newRow("empty") << "";
        QTest::newRow("letter alone") << "T";
        QTest::newRow("shift letter") << "Shift+A";
        QTest::newRow("chord") << "Ctrl+K, Ctrl+T";
    }

    void rejectsUnsafeSequences()
    {
        QFETCH(QString, sequence);
        QString error;
        QVERIFY(!toNativeHotkey(QKeySequence(sequence), &error));
        QVERIFY(!error.isEmpty());
    }

    void allowsStandaloneFunctionKeys()
    {
        QVERIFY(toNativeHotkey(QKeySequence(QStringLiteral("F8"))));
        QVERIFY(toNativeHotkey(QKeySequence(QStringLiteral("Pause"))));
    }
};

QTEST_GUILESS_MAIN(TestKeyMapping)
#include "tst_keymapping.moc"
