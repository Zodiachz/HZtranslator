#pragma once

#include "app/AppContext.h"
#include "core/AppSettings.h"

#include <QPointer>
#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;

namespace hypers {

class HotkeyEdit;
class LanguageComboBox;
class TranslationReply;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(const AppContext& context, QWidget* parent = nullptr);

    // Scrolls to and focuses the API key field of the active provider.
    void focusApiKey();

private:
    struct KeyRow {
        QString providerId;
        QLineEdit* edit = nullptr;
        QPushButton* show = nullptr;
        QPushButton* save = nullptr;
        QPushButton* test = nullptr;
        QPushButton* remove = nullptr;
        QLabel* status = nullptr;
        QPointer<TranslationReply> testReply;
    };
    struct HotkeyRow {
        HotkeyAction action;
        HotkeyEdit* edit = nullptr;
    };

    QWidget* buildTranslationCard();
    QWidget* buildHotkeyCard();
    QWidget* buildBehaviorCard();
    QWidget* buildOcrCard();
    QWidget* buildSystemCard();
    void addKeyRow(QFormLayout* form, const QString& providerId, const QString& label, const QString& hint);

    void saveKey(std::size_t index);
    void testKey(std::size_t index);
    void removeKey(std::size_t index);
    void refreshKeyState(std::size_t index);

    void applyHotkey(const HotkeyRow& row, const QKeySequence& sequence);
    void reloadOcrLanguages();
    void syncLanguageCombos();

    AppContext m_ctx;
    QScrollArea* m_scroll = nullptr;
    QComboBox* m_provider = nullptr;
    LanguageComboBox* m_target = nullptr;
    LanguageComboBox* m_source = nullptr;
    std::vector<KeyRow> m_keys;
    std::vector<HotkeyRow> m_hotkeys;
    QLabel* m_hotkeyStatus = nullptr;
    QComboBox* m_ocrLanguage = nullptr;
    QLabel* m_ocrHint = nullptr;
    QCheckBox* m_autostart = nullptr;
};

} // namespace hypers
