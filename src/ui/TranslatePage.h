#pragma once

#include "app/AppContext.h"

#include <QPointer>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace hypers {

class LanguageComboBox;
class TranslationReply;

// Manual translation: type/paste text, pick languages, translate.
class TranslatePage : public QWidget {
    Q_OBJECT
public:
    explicit TranslatePage(const AppContext& context, QWidget* parent = nullptr);

    void setSourceText(const QString& text, bool translate);
    void focusInput();
    void refreshHotkeyHints();

private:
    void translateNow();
    void onFinished(TranslationReply* reply);
    void swapLanguages();
    void applyProviderFilter();
    void syncFromSettings();
    void updateCounter();
    void setStatus(const QString& text, bool error = false);

    AppContext m_ctx;
    LanguageComboBox* m_source = nullptr;
    LanguageComboBox* m_target = nullptr;
    QPlainTextEdit* m_input = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QPushButton* m_translate = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_clear = nullptr;
    QPushButton* m_swap = nullptr;
    QLabel* m_provider = nullptr;
    QLabel* m_counter = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_selectionHint = nullptr;
    QLabel* m_typedHint = nullptr;
    QLabel* m_ocrHint = nullptr;
    QPointer<TranslationReply> m_reply;
    QString m_lastDetected;
};

} // namespace hypers
