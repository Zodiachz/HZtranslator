#include "features/OcrTranslator.h"

#include "core/AppSettings.h"
#include "core/HistoryStore.h"
#include "features/FeatureCommon.h"
#include "ui/RegionSelector.h"
#include "ui/ResultPopup.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

Q_LOGGING_CATEGORY(lcOcrFlow, "hypers.ocrflow")

namespace hypers {

namespace {
constexpr int kPopupHideDelayMs = 160; // let DWM finish hiding our popup before the screenshot
}

OcrTranslator::OcrTranslator(AppSettings* settings, TranslationService* service, HistoryStore* history,
                             ResultPopup* popup, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_service(service)
    , m_history(history)
    , m_popup(popup)
    , m_selector(new RegionSelector(this))
{
    connect(m_selector, &RegionSelector::regionSelected, this, &OcrTranslator::onRegionSelected);
    connect(&m_ocrWatcher, &QFutureWatcher<OcrOutput>::finished, this, &OcrTranslator::onOcrFinished);
    connect(m_popup, &ResultPopup::canceled, this, [this] {
        // Windows OCR can't be interrupted; its result is discarded instead.
        if (m_ocrWatcher.isRunning())
            m_ocrCanceled = true;
        if (m_reply)
            m_reply->abort();
    });
}

OcrTranslator::~OcrTranslator()
{
    if (m_reply)
        m_reply->abort();
    m_ocrWatcher.waitForFinished();
}

bool OcrTranslator::isBusy() const
{
    return m_selector->isActive() || m_ocrWatcher.isRunning();
}

void OcrTranslator::trigger()
{
    if (isBusy())
        return;
    if (m_reply) {
        m_reply->abort(); // a new capture supersedes a pending translation
        m_reply = nullptr;
    }
    if (m_popup->isVisible()) {
        m_popup->hide();
        QTimer::singleShot(kPopupHideDelayMs, this, &OcrTranslator::startSelection);
        return;
    }
    startSelection();
}

void OcrTranslator::startSelection()
{
    if (!m_selector->isActive())
        m_selector->start();
}

void OcrTranslator::onRegionSelected(const QImage& image, const QRect& globalRect)
{
    m_anchor = globalRect;
    m_recognizedText.clear();
    m_ocrCanceled = false;
    m_popup->showLoading(QStringLiteral("Recognizing text"), m_anchor);
    qCInfo(lcOcrFlow) << "OCR on" << image.size() << "px region";
    m_ocrWatcher.setFuture(QtConcurrent::run(&WindowsOcrEngine::recognize, image, m_settings->ocrLanguage(),
                                             m_settings->ocrMergeLines()));
}

void OcrTranslator::onOcrFinished()
{
    if (m_ocrCanceled) {
        m_ocrCanceled = false;
        return;
    }
    const OcrOutput output = m_ocrWatcher.result();
    if (!output.ok) {
        m_popup->showError(output.error, m_anchor);
        return;
    }
    if (output.text.trimmed().isEmpty()) {
        m_popup->showError(QStringLiteral("No text found in the selected area. Try a larger region, or check the "
                                          "OCR language in Settings › OCR."),
                           m_anchor);
        return;
    }

    m_recognizedText = output.text;
    m_popup->updateStatus(QStringLiteral("Translating"));

    TranslationRequest request;
    request.text = output.text;
    request.sourceLanguage = m_settings->sourceLanguage();
    request.targetLanguage = m_settings->targetLanguage();

    TranslationReply* reply = m_service->translate(request, this);
    m_reply = reply;
    connect(reply, &TranslationReply::retrying, this, [this, reply](int attempt, int, const QString&) {
        if (m_reply == reply)
            m_popup->updateStatus(QStringLiteral("Connection problem - retrying (attempt %1)").arg(attempt));
    });
    connect(reply, &TranslationReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_reply != reply)
            return; // superseded
        m_reply = nullptr;
        onTranslated(reply);
    });
}

void OcrTranslator::onTranslated(TranslationReply* reply)
{
    if (!reply->isSuccess()) {
        if (reply->error().kind != TranslationErrorKind::Canceled)
            m_popup->showError(describeError(m_service, reply), m_anchor, m_recognizedText);
        return;
    }
    recordHistory(m_history, m_settings, QStringLiteral("ocr"), QString(), reply);
    m_popup->showResult(describeTranslation(m_service, reply), m_recognizedText, reply->result().translatedText,
                        m_anchor);
}

} // namespace hypers
