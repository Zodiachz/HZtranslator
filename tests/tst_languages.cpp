#include "core/Languages.h"

#include <QSet>
#include <QtTest>

using namespace hypers;

class TestLanguages : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void codesAreUnique()
    {
        QSet<QString> seen;
        for (const LanguageInfo& lang : Languages::all()) {
            QVERIFY2(!seen.contains(lang.code), qPrintable(lang.code));
            seen.insert(lang.code);
            QVERIFY(!lang.google.isEmpty()); // every language is at least supported by Google
        }
    }

    void findIsCaseInsensitive()
    {
        QVERIFY(Languages::find(QStringLiteral("EN-us")));
        QCOMPARE(Languages::find(QStringLiteral("ar"))->deeplTarget, QStringLiteral("AR"));
        QVERIFY(!Languages::find(QStringLiteral("xx")));
    }

    void providerCodesMapToNames_data()
    {
        QTest::addColumn<QString>("code");
        QTest::addColumn<QString>("name");
        QTest::newRow("deepl EN") << "EN" << "English";
        QTest::newRow("deepl PT-BR") << "PT-BR" << "Portuguese";
        QTest::newRow("google zh-CN") << "zh-CN" << "Chinese";
        QTest::newRow("google legacy iw") << "iw" << "Hebrew";
        QTest::newRow("google ar") << "ar" << "Arabic";
        QTest::newRow("unknown region") << "de-AT" << "German";
        QTest::newRow("unknown") << "xh" << "XH";
    }

    void providerCodesMapToNames()
    {
        QFETCH(QString, code);
        QFETCH(QString, name);
        QCOMPARE(Languages::nameForProviderCode(code), name);
    }

    void rightToLeft()
    {
        QVERIFY(Languages::isRightToLeft(QStringLiteral("ar")));
        QVERIFY(Languages::isRightToLeft(QStringLiteral("fa")));
        QVERIFY(!Languages::isRightToLeft(QStringLiteral("de")));
    }
};

QTEST_GUILESS_MAIN(TestLanguages)
#include "tst_languages.moc"
