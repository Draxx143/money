// Entry point.
//
// Startup order matters: the configuration has to exist before the identity is
// known, and the identity has to exist before the theme, the window title and
// the window icon can be set.
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>

#include "application/Application.h"
#include "application/AppIdentity.h"
#include "application/Settings.h"
#include "core/Logging.h"
#include "core/Paths.h"
#include <eda/Version.h>
#include "ui/MainWindow.h"
#include "ui/Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral(EDA_PACKAGE_NAME));
    QCoreApplication::setApplicationName(QStringLiteral(EDA_PACKAGE_NAME));

    eda::Application application;

    QString error;
    if (!application.initialize(&error)) {
        QMessageBox::critical(nullptr, QCoreApplication::translate("main", "Cannot start"),
                              error);
        return 1;
    }

    const QColor accent = application.identity()->identity().accent;
    const QColor accentSecondary = application.identity()->identity().accentSecondary;
    eda::applyTheme(&app, eda::themeColors(accent, accentSecondary));
    app.setWindowIcon(application.identity()->icon());
    app.setApplicationDisplayName(application.identity()->name());
    app.setApplicationVersion(application.applicationVersion());
    const QString organization = application.identity()->identity().organization;
    QCoreApplication::setOrganizationName(organization.trimmed().isEmpty()
                                              ? QStringLiteral(EDA_PACKAGE_NAME)
                                              : organization.trimmed());

    // Parsed after the identity is known: --version prints the version of the
    // application, and --help has to be able to show the configured name.
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate(
        "main", "PCB inspection, documentation and diagnostics workbench"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({ QStringLiteral("project"),
                       QCoreApplication::translate("main", "Open this project file on startup."),
                       QStringLiteral("file") });
    parser.process(app);

    auto *window = new eda::MainWindow();
    window->show();

    const QString projectPath = parser.value(QStringLiteral("project"));
    if (!projectPath.isEmpty())
        QTimer::singleShot(0, window, [window, projectPath]() { window->openProjectPath(projectPath); });

    const int result = app.exec();
    delete window;
    application.shutdown();
    return result;
}
