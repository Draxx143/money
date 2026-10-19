// Application services: paths, configuration, identity, logging and the
// currently open project. Everything the user interface needs is reached
// through this object, which keeps the ownership explicit and makes the
// services easy to replace in tests.
#pragma once

#include <QObject>
#include <QString>

namespace eda {

class AppIdentity;
class Database;
class Project;
class Settings;

class Application : public QObject
{
    Q_OBJECT
public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    static Application *instance();

    // Loads the configuration, prepares the directories and the log file.
    // Returns false (with a human readable reason) when the application cannot
    // start, e.g. when the home directory is not writable.
    bool initialize(QString *error = nullptr);
    void shutdown();

    Settings *settings() const { return m_settings; }
    Database *database() const { return m_database; }
    AppIdentity *identity() const { return m_identity; }
    Project *project() const { return m_project; }

    QString applicationVersion() const;

signals:
    void identityChanged();

private:
    Settings *m_settings = nullptr;
    Database *m_database = nullptr;
    AppIdentity *m_identity = nullptr;
    Project *m_project = nullptr;
    bool m_initialized = false;
};

} // namespace eda
