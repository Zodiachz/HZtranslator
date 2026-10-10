#include "translation/RetryPolicy.h"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QtTest>

using namespace hypers;

class TestRetryPolicy : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void exponentialWithoutJitter()
    {
        RetryPolicy policy;
        policy.baseDelayMs = 500;
        policy.maxDelayMs = 8000;
        // random01 = 0.5 means zero jitter
        QCOMPARE(policy.delayMs(1, std::nullopt, 0.5), 500);
        QCOMPARE(policy.delayMs(2, std::nullopt, 0.5), 1000);
        QCOMPARE(policy.delayMs(3, std::nullopt, 0.5), 2000);
        QCOMPARE(policy.delayMs(10, std::nullopt, 0.5), 8000); // capped
    }

    void jitterStaysInBounds()
    {
        RetryPolicy policy;
        policy.jitterRatio = 0.2;
        QCOMPARE(policy.delayMs(1, std::nullopt, 0.0), 400);
        QCOMPARE(policy.delayMs(1, std::nullopt, 0.999999), 600);
        for (int i = 0; i < 200; ++i) {
            const int d = policy.delayMs(2);
            QVERIFY(d >= 800 && d <= 1200);
        }
    }

    void retryAfterWins()
    {
        RetryPolicy policy;
        QCOMPARE(policy.delayMs(1, 5000, 0.5), 5000);
        QCOMPARE(policy.delayMs(1, 999999, 0.5), RetryPolicy::kMaxRetryAfterMs);
        QCOMPARE(policy.delayMs(3, 100, 0.5), 2000); // never shorter than the backoff
    }

    void parsesRetryAfter()
    {
        QCOMPARE(RetryPolicy::parseRetryAfter("7"), std::optional<int>(7000));
        QCOMPARE(RetryPolicy::parseRetryAfter(" 0 "), std::optional<int>(0));
        QCOMPARE(RetryPolicy::parseRetryAfter(""), std::optional<int>());
        QCOMPARE(RetryPolicy::parseRetryAfter("soon"), std::optional<int>());

        const QDateTime future = QDateTime::currentDateTimeUtc().addSecs(20);
        const QByteArray httpDate =
            QLocale::c().toString(future, QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'")).toLatin1();
        const std::optional<int> parsed = RetryPolicy::parseRetryAfter(httpDate);
        QVERIFY(parsed.has_value());
        QVERIFY2(*parsed > 15000 && *parsed <= 20000, qPrintable(QString::number(*parsed)));
    }
};

QTEST_GUILESS_MAIN(TestRetryPolicy)
#include "tst_retrypolicy.moc"
