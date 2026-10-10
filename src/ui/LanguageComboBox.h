#pragma once

#include <QComboBox>

#include <functional>

namespace hypers {

// Combo box listing app languages, optionally with "Detect language" and
// filtered to what the active provider supports.
class LanguageComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit LanguageComboBox(bool includeAuto, QWidget* parent = nullptr);

    QString currentCode() const;
    void setCurrentCode(const QString& code);

    // Rebuilds the list keeping the current selection when still available.
    void setFilter(const std::function<bool(const QString& code)>& isSupported);

Q_SIGNALS:
    void codeChanged(const QString& code);

private:
    void rebuild();

    bool m_includeAuto;
    std::function<bool(const QString&)> m_isSupported;
};

} // namespace hypers
