#pragma once

#include "platform/win/WindowsOcrEngine.h"
#include "translation/TranslationService.h"

#include <QFutureWatcher>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRect>

namespace hypers {

class AppSettings;
class HistoryStore;
class RegionSelector;
class ResultPopup;

// Screen OCR workflow: region selection -> Windows OCR (worker thread) ->
// translation -> floating popup next to the selected region.
class OcrTranslator : public QObject {
    Q_OBJECT
public:
    OcrTranslator(AppSettings* settings, TranslationService* service, HistoryStore* history, ResultPopup* popup,
                  QObject* parent = nullptr);
    ~OcrTranslator() override;

    void trigger();
    bool isBusy() const;

Q_SIGNALS:
    void notification(const QString& title, const QString& message, bool isError);

private:
    void startSelection();
    void onRegionSelected(const QImage& image, const QRect& globalRect);
    void onOcrFinished();
    void onTranslated(TranslationReply* reply);

    AppSettings* m_settings;
    TranslationService* m_service;
    HistoryStore* m_history;
    ResultPopup* m_popup;
    RegionSelector* m_selector;
    QFutureWatcher<OcrOutput> m_ocrWatcher;
    QPointer<TranslationReply> m_reply;
    QRect m_anchor;
    QString m_recognizedText;
};

} // namespace hypers
