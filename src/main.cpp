#include "Version.h"
#include "app/HypersApp.h"
#include "core/Logger.h"
#include "core/SingleInstance.h"
#include "platform/win/AutoStart.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QLoggingCategory>

#include <windows.h>

int main(int argc, char* argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral(HYPERS_ORGANIZATION));
    QApplication::setApplicationName(QStringLiteral(HYPERS_APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(HYPERS_VERSION_STRING));
    QApplication::setQuitOnLastWindowClosed(false); // lives in the tray

    hypers::Logger::install();
    qInfo() << "HYPERS TRANSLATOR" << HYPERS_VERSION_STRING << "starting";

    // One instance per Windows session (fast user switching / RDP get their own).
    DWORD sessionId = 0;
    ::ProcessIdToSessionId(::GetCurrentProcessId(), &sessionId);
    hypers::SingleInstance instance(QStringLiteral("HypersTranslator-%1").arg(sessionId));
    if (!instance.acquire()) {
        instance.notifyPrimary();
        return 0;
    }

    hypers::theme::apply(app);

    const bool launchedAtLogin = app.arguments().contains(QLatin1String(hypers::autostart::kLaunchArgument));
    hypers::HypersApp hypersApp;
    if (!hypersApp.initialize(launchedAtLogin))
        return 1;
    QObject::connect(&instance, &hypers::SingleInstance::activationRequested, &hypersApp,
                     &hypers::HypersApp::showMainWindow);

    const int code = app.exec();
    qInfo() << "Exited with code" << code;
    return code;
}
