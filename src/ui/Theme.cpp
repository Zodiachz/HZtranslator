#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

namespace hypers::theme {

void apply(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, background());
    p.setColor(QPalette::WindowText, text());
    p.setColor(QPalette::Base, surface());
    p.setColor(QPalette::AlternateBase, surfaceRaised());
    p.setColor(QPalette::ToolTipBase, surfaceRaised());
    p.setColor(QPalette::ToolTipText, text());
    p.setColor(QPalette::PlaceholderText, textMuted());
    p.setColor(QPalette::Text, text());
    p.setColor(QPalette::Button, surfaceRaised());
    p.setColor(QPalette::ButtonText, text());
    p.setColor(QPalette::BrightText, accentHover());
    p.setColor(QPalette::Highlight, accent());
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, accentHover());
    p.setColor(QPalette::Disabled, QPalette::Text, textMuted().darker(140));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, textMuted().darker(140));
    p.setColor(QPalette::Disabled, QPalette::WindowText, textMuted().darker(140));
    app.setPalette(p);

    // Segoe UI Variable is the Windows 11 system font; Segoe UI is the Windows 10 fallback.
    QFont font;
    const QStringList families = QFontDatabase::families();
    font.setFamily(families.contains(QStringLiteral("Segoe UI Variable Text")) ? QStringLiteral("Segoe UI Variable Text")
                                                                               : QStringLiteral("Segoe UI"));
    font.setPointSizeF(10.0);
    app.setFont(font);

    QFile qss(QStringLiteral(":/styles/dark.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    app.setWindowIcon(appIcon());
}

QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addFile(QStringLiteral(":/icons/app-%1.png").arg(size), QSize(size, size));
    return icon;
}

QIcon trayIcon(bool paused)
{
    if (paused)
        return QIcon(QStringLiteral(":/icons/tray-paused-32.png"));
    QIcon icon;
    for (int size : {16, 24, 32, 48})
        icon.addFile(QStringLiteral(":/icons/app-%1.png").arg(size), QSize(size, size));
    return icon;
}

} // namespace hypers::theme
