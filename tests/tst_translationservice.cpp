// End-to-end tests of TranslationService (batching, retries, timeouts, cache,
// cancellation) against a local mock HTTP server - no real API calls.

#include "translation/TranslationService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include <functional>

using namespace hypers;

namespace {

struct MockResponse {
    int status = 200;
    QByteArray body;
    QByteArray extraHeaders;
    bool hang = false; // never answer (for timeout tests)
};

class MockHttpServer : public QObject {
public:
    explicit MockHttpServer(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_server.listen(QHostAddress::LocalHost, 0);
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection())
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
        });
    }

    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/translate").arg(m_server.serverPort())); }

    // Responses served first, in order; afterwards `fallback` answers.
    QList<MockResponse> queue;
    // Default: echo every segment upper-cased.
    std::function<MockResponse(const QByteArray& body)> fallback = [](const QByteArray& body) {
        const QJsonArray q = QJsonDocument::fromJson(body).object().value(QStringLiteral("q")).toArray();
        QJsonArray out;
        for (const QJsonValue& v : q)
            out.append(v.toString().toUpper());
        QJsonObject obj;
        obj.insert(QStringLiteral("t"), out);
        obj.insert(QStringLiteral("d"), QStringLiteral("EN"));
        return MockResponse{200, QJsonDocument(obj).toJson(QJsonDocument::Compact), {}, false};
    };
    int requestCount = 0;
    QList<QByteArray> bodies;

private:
    void onReadyRead(QTcpSocket* socket)
    {
        QByteArray& buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;
        qsizetype contentLength = 0;
        for (const QByteArray& line : buffer.left(headerEnd).split('\n')) {
            if (line.toLower().startsWith("content-length:"))
                contentLength = line.mid(15).trimmed().toLongLong();
        }
        if (buffer.size() < headerEnd + 4 + contentLength)
            return;
        const QByteArray body = buffer.mid(headerEnd + 4, contentLength);
        m_buffers.remove(socket);

        ++requestCount;
        bodies.append(body);
        const MockResponse response = queue.isEmpty() ? fallback(body) : queue.takeFirst();
        if (response.hang)
            return;
        QByteArray raw = "HTTP/1.1 " + QByteArray::number(response.status) + " Mock\r\n"
            + "Content-Type: application/json\r\n" + "Content-Length: " + QByteArray::number(response.body.size())
            + "\r\n" + "Connection: close\r\n" + response.extraHeaders + "\r\n" + response.body;
        socket->write(raw);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
};

class FakeProvider final : public ITranslationProvider {
public:
    explicit FakeProvider(QUrl url)
        : m_url(std::move(url))
    {
    }

    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    void setApiKey(const QString& key) override { m_key = key; }
    bool hasApiKey() const override { return !m_key.isEmpty(); }
    int maxSegmentsPerRequest() const override { return maxSegments; }
    int maxCharactersPerRequest() const override { return maxChars; }
    bool supportsSourceLanguage(const QString&) const override { return true; }
    bool supportsTargetLanguage(const QString& code) const override { return code != QLatin1String("xx"); }

    QNetworkRequest buildRequest() const override
    {
        QNetworkRequest request(m_url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return request;
    }

    QByteArray buildPayload(const QStringList& segments, const QString&, const QString& target) const override
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("q"), QJsonArray::fromStringList(segments));
        obj.insert(QStringLiteral("target"), target);
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    ProviderResponse parseResponse(int status, const QByteArray& body) const override
    {
        ProviderResponse r;
        if (status != 200) {
            r.error = errorForHttpStatus(status, QString::fromUtf8(body));
            return r;
        }
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        for (const QJsonValue& v : obj.value(QStringLiteral("t")).toArray())
            r.translations.append(v.toString());
        r.detectedSourceLanguage = obj.value(QStringLiteral("d")).toString();
        r.ok = true;
        return r;
    }

    int maxSegments = 10;
    int maxChars = 1000;

private:
    QUrl m_url;
    QString m_key;
};

TranslationRequest request(const QString& text)
{
    TranslationRequest r;
    r.text = text;
    r.targetLanguage = QStringLiteral("de");
    return r;
}

} // namespace

class TestTranslationService : public QObject {
    Q_OBJECT
private:
    MockHttpServer* m_server = nullptr;
    TranslationService* m_service = nullptr;
    FakeProvider* m_provider = nullptr;

    // Runs a translation to completion and returns the (finished) reply.
    TranslationReply* run(const TranslationRequest& r, int timeoutMs = 10000)
    {
        TranslationReply* reply = m_service->translate(r, this);
        QSignalSpy finished(reply, &TranslationReply::finished);
        if (!finished.wait(timeoutMs))
            qWarning() << "translation did not finish";
        return reply;
    }

private Q_SLOTS:
    void initTestCase() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }

    void init()
    {
        m_server = new MockHttpServer(this);
        m_service = new TranslationService(this);
        auto provider = std::make_unique<FakeProvider>(m_server->url());
        provider->setApiKey(QStringLiteral("secret"));
        m_provider = provider.get();
        m_service->registerProvider(std::move(provider));
        RetryPolicy policy;
        policy.baseDelayMs = 10;
        policy.maxDelayMs = 50;
        policy.maxRetries = 3;
        m_service->setRetryPolicy(policy);
        m_service->setRequestTimeoutMs(3000);
    }

    void cleanup()
    {
        delete m_service;
        delete m_server;
    }

    void translatesAndKeepsWhitespace()
    {
        TranslationReply* reply = run(request(QStringLiteral("  hello world\n")));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->error().message));
        QCOMPARE(reply->result().translatedText, QStringLiteral("  HELLO WORLD\n"));
        QCOMPARE(reply->result().detectedSourceLanguage, QStringLiteral("EN"));
        QCOMPARE(reply->result().providerId, QStringLiteral("fake"));
        QCOMPARE(m_server->requestCount, 1);
        // Only the core is sent, not the whitespace.
        QVERIFY(m_server->bodies.first().contains("\"hello world\""));
    }

    void retriesTransientErrors()
    {
        m_server->queue = {MockResponse{503, "busy", {}, false},
                           MockResponse{429, "slow down", "Retry-After: 0\r\n", false}};
        TranslationReply* reply = m_service->translate(request(QStringLiteral("abc")), this);
        QSignalSpy retrying(reply, &TranslationReply::retrying);
        QSignalSpy finished(reply, &TranslationReply::finished);
        QVERIFY(finished.wait(10000));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->error().message));
        QCOMPARE(reply->result().translatedText, QStringLiteral("ABC"));
        QCOMPARE(retrying.count(), 2);
        QCOMPARE(m_server->requestCount, 3);
        QCOMPARE(finished.count(), 1);
    }

    void doesNotRetryAuthErrors()
    {
        m_server->queue = {MockResponse{401, "bad key", {}, false}};
        TranslationReply* reply = run(request(QStringLiteral("abc")));
        QVERIFY(!reply->isSuccess());
        QCOMPARE(reply->error().kind, TranslationErrorKind::Authentication);
        QCOMPARE(m_server->requestCount, 1);
    }

    void givesUpAfterMaxRetries()
    {
        RetryPolicy policy = m_service->retryPolicy();
        policy.maxRetries = 2;
        m_service->setRetryPolicy(policy);
        for (int i = 0; i < 5; ++i)
            m_server->queue.append(MockResponse{500, "down", {}, false});
        TranslationReply* reply = run(request(QStringLiteral("abc")));
        QCOMPARE(reply->error().kind, TranslationErrorKind::ServerError);
        QCOMPARE(m_server->requestCount, 3);
    }

    void timesOut()
    {
        RetryPolicy policy = m_service->retryPolicy();
        policy.maxRetries = 1;
        m_service->setRetryPolicy(policy);
        m_service->setRequestTimeoutMs(250);
        m_server->queue = {MockResponse{200, {}, {}, true}, MockResponse{200, {}, {}, true}};
        TranslationReply* reply = run(request(QStringLiteral("abc")));
        QCOMPARE(reply->error().kind, TranslationErrorKind::Timeout);
        QCOMPARE(m_server->requestCount, 2);
    }

    void batchesLongText()
    {
        m_provider->maxChars = 60;
        m_provider->maxSegments = 2;
        QString text;
        for (int i = 0; i < 12; ++i)
            text += QStringLiteral("this is sentence %1. ").arg(i);
        text += QStringLiteral("\n\nfinal paragraph.\n");
        TranslationReply* reply = run(request(text));
        QVERIFY2(reply->isSuccess(), qPrintable(reply->error().message));
        QCOMPARE(reply->result().translatedText, text.toUpper());
        QVERIFY(m_server->requestCount > 1);
    }

    void cachesResults()
    {
        TranslationReply* first = run(request(QStringLiteral("cache me")));
        QVERIFY(first->isSuccess());
        QVERIFY(!first->result().fromCache);
        TranslationReply* second = run(request(QStringLiteral("cache me")));
        QVERIFY(second->isSuccess());
        QVERIFY(second->result().fromCache);
        QCOMPARE(second->result().translatedText, QStringLiteral("CACHE ME"));
        QCOMPARE(m_server->requestCount, 1);
    }

    void validatesInput()
    {
        TranslationReply* empty = run(request(QStringLiteral("   \n")));
        QCOMPARE(empty->error().kind, TranslationErrorKind::InvalidInput);

        TranslationRequest unsupported = request(QStringLiteral("hi"));
        unsupported.targetLanguage = QStringLiteral("xx");
        TranslationReply* reply = run(unsupported);
        QCOMPARE(reply->error().kind, TranslationErrorKind::UnsupportedLanguage);

        m_provider->setApiKey(QString());
        TranslationReply* noKey = run(request(QStringLiteral("hi")));
        QCOMPARE(noKey->error().kind, TranslationErrorKind::NotConfigured);
        QCOMPARE(m_server->requestCount, 0);
    }

    void abortFinishesOnce()
    {
        m_server->queue = {MockResponse{200, {}, {}, true}};
        TranslationReply* reply = m_service->translate(request(QStringLiteral("abc")), this);
        QSignalSpy finished(reply, &TranslationReply::finished);
        QTRY_COMPARE(m_server->requestCount, 1);
        reply->abort();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(reply->error().kind, TranslationErrorKind::Canceled);
        QTest::qWait(100);
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestTranslationService)
#include "tst_translationservice.moc"
