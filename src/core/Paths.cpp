#include "Paths.h"

#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

namespace eda {

QString Paths::m_applicationId = QStringLiteral("pcb-workbench");

QString Paths::applicationId() { return m_applicationId; }

void Paths::setApplicationId(const QString &id)
{
    if (!id.trimmed().isEmpty())
        m_applicationId = id.trimmed();
}

QString Paths::baseDir(const char *envVar, const QString &fallback, const QString &kind)
{
    const QByteArray root = qgetenv("PCB_WORKBENCH_HOME");
    if (!root.isEmpty())
        return QString::fromLocal8Bit(root) + QLatin1Char('/') + kind + QLatin1Char('/')
            + m_applicationId;
    const QByteArray env = qgetenv(envVar);
    if (!env.isEmpty())
        return QString::fromLocal8Bit(env) + QLatin1Char('/') + m_applicationId;
    return QDir::home().absoluteFilePath(fallback + QLatin1Char('/') + m_applicationId);
}

QString Paths::configDir() { return baseDir("XDG_CONFIG_HOME", QStringLiteral(".config"), QStringLiteral("config")); }
QString Paths::dataDir() { return baseDir("XDG_DATA_HOME", QStringLiteral(".local/share"), QStringLiteral("data")); }
QString Paths::cacheDir() { return baseDir("XDG_CACHE_HOME", QStringLiteral(".cache"), QStringLiteral("cache")); }
QString Paths::stateDir() { return baseDir("XDG_STATE_HOME", QStringLiteral(".local/state"), QStringLiteral("state")); }

QString Paths::logDir() { return stateDir() + QStringLiteral("/logs"); }
QString Paths::settingsFile() { return configDir() + QStringLiteral("/settings.conf"); }
QString Paths::databaseFile() { return dataDir() + QStringLiteral("/knowledge.sqlite3"); }
QString Paths::backupDir() { return dataDir() + QStringLiteral("/backups"); }
QString Paths::documentDir() { return dataDir() + QStringLiteral("/documents"); }

QString Paths::defaultProjectDir()
{
    // With a redirected home (PCB_WORKBENCH_HOME, used by the test suite) there
    // is no Documents folder to speak of, and everything the application owns
    // belongs under the data directory anyway.
    if (!qEnvironmentVariableIsEmpty("PCB_WORKBENCH_HOME"))
        return dataDir() + QStringLiteral("/projects");

    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString base = docs.isEmpty() ? QDir::homePath() : docs;
    return base + QStringLiteral("/") + QCoreApplication::translate("Paths", "PCB Projects");
}

QString Paths::workspaceStateFile() { return configDir() + QStringLiteral("/workspace.ini"); }

bool Paths::ensureDirectories(QString *error)
{
    const QStringList dirs = { configDir(), dataDir(),   cacheDir(),  stateDir(),
                               logDir(),    backupDir(), documentDir() };
    for (const QString &dir : dirs) {
        QDir d(dir);
        if (!d.exists() && !d.mkpath(QStringLiteral("."))) {
            if (error)
                *error = QCoreApplication::translate("Paths", "Cannot create directory: %1").arg(dir);
            return false;
        }
    }
    return true;
}

} // namespace eda
