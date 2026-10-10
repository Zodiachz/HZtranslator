#pragma once

#include "translation/ITranslationProvider.h"
#include "translation/RetryPolicy.h"
#include "translation/TextSegmenter.h"
#include "translation/TranslationTypes.h"

#include <QCache>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <map>
#include <memory>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

namespace hypers {

class TranslationService;

// Handle for one asynchronous translation, modelled after QNetworkReply.
// Emits finished() exactly once (also on abort). The caller owns the reply
// and should deleteLater() it from the finished() slot.
class TranslationReply : public QObject {
    Q_OBJECT
public:
    ~TranslationReply() override;

    const TranslationRequest& request() const { return m_request; }
    QString providerId() const { return m_providerId; }

    bool isFinished() const { return m_finished; }
    bool isSuccess() const { return m_finished && m_error.kind == TranslationErrorKind::None; }
    const TranslationResult& result() const { return m_result; }
    const TranslationError& error() const { return m_error; }
    qint64 elapsedMs() const { return m_elapsed.elapsed(); }

    void abort();

Q_SIGNALS:
    void finished();
    // A transient failure happened; the next attempt starts after delayMs.
    void retrying(int attempt, int delayMs, const QString& reason);

private:
    friend class TranslationService;
    TranslationReply(TranslationService* service, ITranslationProvider* provider,
                     const TranslationRequest& request, QObject* parent);

    void start();
    void sendCurrentBatch();
    void onNetworkFinished();
    void handleBatchFailure(const TranslationError& error);
    void finishWithResult();
    void finishWithError(const TranslationError& error);
    void finishWith(const TranslationResult& result); // cache hit

    QPointer<TranslationService> m_service;
    ITranslationProvider* m_provider = nullptr;
    TranslationRequest m_request;
    QString m_providerId;

    QList<TextSegment> m_segments;
    std::vector<std::vector<int>> m_batches; // indices into m_segments
    std::size_t m_batchIndex = 0;
    QStringList m_translatedCores;
    QString m_detectedSource;

    QPointer<QNetworkReply> m_networkReply;
    QTimer m_timeoutTimer;
    QTimer m_retryTimer;
    int m_attempt = 0;      // attempts for the current batch
    bool m_timedOut = false;
    bool m_finished = false;
    QElapsedTimer m_elapsed;

    TranslationResult m_result;
    TranslationError m_error;
};

class TranslationService : public QObject {
    Q_OBJECT
public:
    explicit TranslationService(QObject* parent = nullptr);
    ~TranslationService() override;

    void registerProvider(std::unique_ptr<ITranslationProvider> provider);
    ITranslationProvider* provider(const QString& id) const;
    QList<ITranslationProvider*> providers() const;

    void setActiveProvider(const QString& id);
    QString activeProviderId() const { return m_activeProviderId; }
    ITranslationProvider* activeProvider() const { return provider(m_activeProviderId); }

    void setRetryPolicy(const RetryPolicy& policy) { m_retryPolicy = policy; }
    const RetryPolicy& retryPolicy() const { return m_retryPolicy; }
    void setRequestTimeoutMs(int ms) { m_timeoutMs = ms; }
    int requestTimeoutMs() const { return m_timeoutMs; }

    void clearCache() { m_cache.clear(); }

    // Translates with the active provider. The returned reply is never null.
    TranslationReply* translate(const TranslationRequest& request, QObject* parent = nullptr);
    TranslationReply* translateWith(const QString& providerId, const TranslationRequest& request,
                                    QObject* parent = nullptr);

    static constexpr int kMaxInputCharacters = 100000;

Q_SIGNALS:
    void activeProviderChanged(const QString& id);

private:
    friend class TranslationReply;
    static QString cacheKey(const QString& providerId, const TranslationRequest& request);
    void storeInCache(const QString& providerId, const TranslationRequest& request, const TranslationResult& result);

    QNetworkAccessManager* m_network = nullptr;
    std::map<QString, std::unique_ptr<ITranslationProvider>> m_providers;
    QString m_activeProviderId;
    RetryPolicy m_retryPolicy;
    int m_timeoutMs = 15000;
    QCache<QString, TranslationResult> m_cache{256};
};

} // namespace hypers
