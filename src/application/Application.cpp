#include "Application.h"

#include "AppIdentity.h"
#include "Settings.h"

#include "../core/Logging.h"
#include "../core/Paths.h"
#include "../core/Project.h"
#include <eda/Version.h>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace eda {

namespace {
Application *g_instance = nullptr;
}

Application::Application(QObject *parent) : QObject(parent)
{
    g_instance = this;
}

Application::~Application()
{
    g_instance = nullptr;
}

Application *Application::instance() { return g_instance; }

bool Application::initialize(QString *error)
{
    if (m_initialized)
        return true;

    m_settings = new Settings(this);
    m_identity = new AppIdentity(this);
    m_project = new Project(this);

    if (!Paths::ensureDirectories(error))
        return false;

    installQtMessageHandler();

    if (!m_settings->load(error)) {
        // A corrupt configuration must not stop the application: keep the
        // defaults and tell the user what happened.
        logError(QStringLiteral("settings"), *error);
        if (error)
            error->clear();
    }

    m_identity->load(m_settings);

    Logger::instance().setLogDirectory(
        m_settings->value(QStringLiteral("logging/directory"), Paths::logDir()).toString());
    Logger::instance().setMinimumLevel(
        logLevelFromName(m_settings->value(QStringLiteral("logging/level"), QStringLiteral("info")).toString()));

    QCoreApplication::setApplicationName(m_identity->name());
    if (!m_identity->identity().organization.isEmpty())
        QCoreApplication::setOrganizationName(m_identity->identity().organization);

    connect(m_identity, &AppIdentity::changed, this, [this]() {
        QCoreApplication::setApplicationName(m_identity->name());
        m_identity->save(m_settings);
        emit identityChanged();
    });

    logInfo(QStringLiteral("app"), QStringLiteral("%1 %2 starting (Qt %3)")
                                       .arg(m_identity->name(), applicationVersion(),
                                            QStringLiteral(QT_VERSION_STR)));
    logInfo(QStringLiteral("app"), QStringLiteral("configuration: %1").arg(Paths::settingsFile()));
    logInfo(QStringLiteral("app"), QStringLiteral("log file: %1").arg(Logger::instance().currentLogFile()));

    m_initialized = true;
    return true;
}

void Application::shutdown()
{
    if (!m_initialized)
        return;
    m_identity->save(m_settings);
    QString error;
    if (!m_settings->save(&error))
        logError(QStringLiteral("settings"), error);
    logInfo(QStringLiteral("app"), QStringLiteral("shutting down"));
    m_initialized = false;
}

QString Application::applicationVersion() const { return QStringLiteral(EDA_VERSION_STRING); }

} // namespace eda
