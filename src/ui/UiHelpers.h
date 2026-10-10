#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace hypers::ui {

inline QLabel* label(const QString& text, const char* objectName = nullptr, QWidget* parent = nullptr)
{
    auto* l = new QLabel(text, parent);
    if (objectName)
        l->setObjectName(QLatin1String(objectName));
    return l;
}

// Rounded "card" container with an optional title; returns the card and
// exposes its body layout through `body`.
inline QFrame* card(const QString& title, QVBoxLayout** body, QWidget* parent = nullptr)
{
    auto* frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 18, 20, 20);
    layout->setSpacing(12);
    if (!title.isEmpty())
        layout->addWidget(label(title, "cardTitle"));
    if (body)
        *body = layout;
    return frame;
}

inline QPushButton* button(const QString& text, const char* role = nullptr, QWidget* parent = nullptr)
{
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    if (role)
        b->setProperty(role, true);
    return b;
}

// Keyboard-key looking label, e.g. for "Ctrl+Alt+T".
inline QLabel* kbd(const QString& text, QWidget* parent = nullptr)
{
    auto* l = label(text, "kbd", parent);
    l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return l;
}

// Re-applies the stylesheet after a dynamic property change.
inline void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

inline void setStatus(QLabel* label, const QString& text, bool error)
{
    label->setObjectName(error ? QStringLiteral("errorText") : QStringLiteral("successText"));
    label->setText(text);
    label->setVisible(!text.isEmpty());
    repolish(label);
}

} // namespace hypers::ui
