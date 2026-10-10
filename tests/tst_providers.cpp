#include "translation/DeepLProvider.h"
#include "translation/GoogleTranslateProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace hypers;

namespace {
QJsonObject parseObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}
} // namespace

class TestProviders : public QObject {
    Q_OBJECT
private Q_SLOTS:
    // ---------------- DeepL ----------------
    void deeplEndpointDependsOnKey()
    {
        DeepLProvider provider;
        provider.setApiKey(QStringLiteral("abc:fx"));
        QCOMPARE(provider.buildRequest().url().host(), QStringLiteral("api-free.deepl.com"));
        QCOMPARE(provider.buildRequest().rawHeader("Authorization"), QByteArray("DeepL-Auth-Key abc:fx"));
        provider.setApiKey(QStringLiteral("  pro-key  "));
        QCOMPARE(provider.buildRequest().url().host(), QStringLiteral("api.deepl.com"));
        QCOMPARE(provider.buildRequest().url().path(), QStringLiteral("/v2/translate"));
    }

    void deeplPayload()
    {
        DeepLProvider provider;
        const QJsonObject body = parseObject(provider.buildPayload(
            {QStringLiteral("Hello"), QStringLiteral("World")}, QStringLiteral("auto"), QStringLiteral("en-GB")));
        QCOMPARE(body.value(QStringLiteral("target_lang")).toString(), QStringLiteral("EN-GB"));
        QVERIFY(!body.contains(QStringLiteral("source_lang")));
        QCOMPARE(body.value(QStringLiteral("text")).toArray().size(), 2);

        const QJsonObject withSource = parseObject(
            provider.buildPayload({QStringLiteral("Hi")}, QStringLiteral("pt-BR"), QStringLiteral("ar")));
        QCOMPARE(withSource.value(QStringLiteral("source_lang")).toString(), QStringLiteral("PT"));
        QCOMPARE(withSource.value(QStringLiteral("target_lang")).toString(), QStringLiteral("AR"));
    }

    void deeplParsesSuccess()
    {
        DeepLProvider provider;
        const ProviderResponse r = provider.parseResponse(
            200, R"({"translations":[{"detected_source_language":"EN","text":"مرحبا"},{"text":"عالم"}]})");
        QVERIFY(r.ok);
        QCOMPARE(r.translations, QStringList({QStringLiteral("مرحبا"), QStringLiteral("عالم")}));
        QCOMPARE(r.detectedSourceLanguage, QStringLiteral("EN"));
    }

    void deeplParsesErrors_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("kind");
        QTest::addColumn<bool>("retryable");
        QTest::newRow("auth") << 403 << QByteArray(R"({"message":"Wrong endpoint"})")
                              << int(TranslationErrorKind::Authentication) << false;
        QTest::newRow("quota") << 456 << QByteArray(R"({"message":"Quota exceeded"})")
                               << int(TranslationErrorKind::QuotaExceeded) << false;
        QTest::newRow("rate") << 429 << QByteArray("{}") << int(TranslationErrorKind::RateLimited) << true;
        QTest::newRow("server") << 503 << QByteArray("oops") << int(TranslationErrorKind::ServerError) << true;
        QTest::newRow("too large") << 413 << QByteArray("{}") << int(TranslationErrorKind::InvalidInput) << false;
        QTest::newRow("bad target") << 400 << QByteArray(R"({"message":"Value for 'target_lang' not supported."})")
                                    << int(TranslationErrorKind::UnsupportedLanguage) << false;
        QTest::newRow("garbage 200") << 200 << QByteArray("not json") << int(TranslationErrorKind::InvalidResponse)
                                     << false;
    }

    void deeplParsesErrors()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, kind);
        QFETCH(bool, retryable);
        DeepLProvider provider;
        const ProviderResponse r = provider.parseResponse(status, body);
        QVERIFY(!r.ok);
        QCOMPARE(int(r.error.kind), kind);
        QCOMPARE(r.error.isRetryable(), retryable);
        QVERIFY(!r.error.userMessage().isEmpty());
    }

    void deeplLanguageSupport()
    {
        DeepLProvider provider;
        QVERIFY(provider.supportsTargetLanguage(QStringLiteral("ar")));
        QVERIFY(provider.supportsSourceLanguage(QStringLiteral("auto")));
        QVERIFY(!provider.supportsTargetLanguage(QStringLiteral("hi")));
    }

    // ---------------- Google ----------------
    void googleRequestUsesHeaderKey()
    {
        GoogleTranslateProvider provider;
        provider.setApiKey(QStringLiteral("AIza-test"));
        const QNetworkRequest request = provider.buildRequest();
        QCOMPARE(request.rawHeader("X-Goog-Api-Key"), QByteArray("AIza-test"));
        QVERIFY(!request.url().toString().contains(QStringLiteral("AIza"))); // never in the URL
    }

    void googlePayload()
    {
        GoogleTranslateProvider provider;
        const QJsonObject body = parseObject(
            provider.buildPayload({QStringLiteral("Hallo")}, QStringLiteral("de"), QStringLiteral("zh-Hant")));
        QCOMPARE(body.value(QStringLiteral("target")).toString(), QStringLiteral("zh-TW"));
        QCOMPARE(body.value(QStringLiteral("source")).toString(), QStringLiteral("de"));
        QCOMPARE(body.value(QStringLiteral("format")).toString(), QStringLiteral("text"));
        QCOMPARE(body.value(QStringLiteral("q")).toArray().first().toString(), QStringLiteral("Hallo"));
    }

    void googleParsesSuccess()
    {
        GoogleTranslateProvider provider;
        const ProviderResponse r = provider.parseResponse(
            200, R"({"data":{"translations":[{"translatedText":"Hello <b>","detectedSourceLanguage":"de"}]}})");
        QVERIFY(r.ok);
        QCOMPARE(r.translations.first(), QStringLiteral("Hello <b>"));
        QCOMPARE(r.detectedSourceLanguage, QStringLiteral("de"));
    }

    void googleParsesErrors_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("kind");
        QTest::newRow("invalid key") << 400
                                     << QByteArray(R"({"error":{"code":400,"message":"API key not valid. Please pass a valid API key.","status":"INVALID_ARGUMENT","details":[{"reason":"API_KEY_INVALID"}]}})")
                                     << int(TranslationErrorKind::Authentication);
        QTest::newRow("api disabled") << 403
                                      << QByteArray(R"({"error":{"code":403,"message":"Cloud Translation API has not been used","status":"PERMISSION_DENIED","details":[{"reason":"SERVICE_DISABLED"}]}})")
                                      << int(TranslationErrorKind::Authentication);
        QTest::newRow("daily limit") << 403
                                     << QByteArray(R"({"error":{"code":403,"message":"Daily Limit Exceeded","errors":[{"reason":"dailyLimitExceeded"}],"status":"PERMISSION_DENIED"}})")
                                     << int(TranslationErrorKind::QuotaExceeded);
        QTest::newRow("rate limit") << 403
                                    << QByteArray(R"({"error":{"code":403,"message":"User Rate Limit Exceeded","errors":[{"reason":"userRateLimitExceeded"}]}})")
                                    << int(TranslationErrorKind::RateLimited);
        QTest::newRow("bad language") << 400
                                      << QByteArray(R"({"error":{"code":400,"message":"Bad language pair: xx","status":"INVALID_ARGUMENT"}})")
                                      << int(TranslationErrorKind::UnsupportedLanguage);
        QTest::newRow("server") << 500 << QByteArray() << int(TranslationErrorKind::ServerError);
    }

    void googleParsesErrors()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(int, kind);
        GoogleTranslateProvider provider;
        const ProviderResponse r = provider.parseResponse(status, body);
        QVERIFY(!r.ok);
        QCOMPARE(int(r.error.kind), kind);
        QCOMPARE(r.error.httpStatus, status);
    }
};

QTEST_GUILESS_MAIN(TestProviders)
#include "tst_providers.moc"
