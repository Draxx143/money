// Path layout of the application.
//
// Everything the application writes lives in user writable locations:
//
//   configuration   $XDG_CONFIG_HOME/<app-id>      (~/.config/<app-id>)
//   data            $XDG_DATA_HOME/<app-id>        (~/.local/share/<app-id>)
//   cache           $XDG_CACHE_HOME/<app-id>       (~/.cache/<app-id>)
//   state / logs    $XDG_STATE_HOME/<app-id>/logs  (~/.local/state/<app-id>/logs)
//
// Nothing is ever written into the installation directory, so a normal user can
// run the packaged application without root privileges.
#pragma once

#include <QString>

namespace eda {

class Paths
{
public:
    // Internal identifier used for the directory names. It is deliberately not
    // the configurable display name, so renaming the application in the
    // settings never orphans existing configuration, projects or databases.
    static QString applicationId();
    static void setApplicationId(const QString &id);

    static QString configDir();
    static QString dataDir();
    static QString cacheDir();
    static QString stateDir();

    static QString logDir();
    static QString settingsFile();
    static QString databaseFile();
    static QString backupDir();
    static QString documentDir();

    static QString defaultProjectDir();
    static QString workspaceStateFile();

    // Creates every directory the application needs. Returns false (and records
    // the reason) when a directory cannot be created.
    static bool ensureDirectories(QString *error = nullptr);

private:
    // Directory for one XDG category. The environment variable
    // PCB_WORKBENCH_HOME overrides everything and is used by the test suite to
    // work in a temporary directory instead of the user's home.
    static QString baseDir(const char *envVar, const QString &fallback, const QString &kind);
    static QString m_applicationId;
};

} // namespace eda
