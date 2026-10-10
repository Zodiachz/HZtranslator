#include "ui/LanguageComboBox.h"

#include "core/Languages.h"

#include <QSignalBlocker>

namespace hypers {

LanguageComboBox::LanguageComboBox(bool includeAuto, QWidget* parent)
    : QComboBox(parent)
    , m_includeAuto(includeAuto)
{
    setMaxVisibleItems(16);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    setMinimumContentsLength(18);
    rebuild();
    connect(this, &QComboBox::currentIndexChanged, this, [this](int) { Q_EMIT codeChanged(currentCode()); });
}

QString LanguageComboBox::currentCode() const
{
    return currentData().toString();
}

void LanguageComboBox::setCurrentCode(const QString& code)
{
    const int index = findData(code);
    if (index >= 0)
        setCurrentIndex(index);
}

void LanguageComboBox::setFilter(const std::function<bool(const QString&)>& isSupported)
{
    m_isSupported = isSupported;
    rebuild();
}

void LanguageComboBox::rebuild()
{
    const QString previous = currentCode();
    {
        const QSignalBlocker blocker(this);
        clear();
        if (m_includeAuto)
            addItem(Languages::label(QString::fromLatin1(Languages::kAuto)), QString::fromLatin1(Languages::kAuto));
        for (const LanguageInfo& lang : Languages::all()) {
            if (m_isSupported && !m_isSupported(lang.code))
                continue;
            addItem(Languages::label(lang.code), lang.code);
        }
        const int index = findData(previous);
        setCurrentIndex(index >= 0 ? index : 0);
    }
    if (currentCode() != previous)
        Q_EMIT codeChanged(currentCode());
}

} // namespace hypers
