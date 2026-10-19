// Shared test scaffolding.
//
// Every test runs with PCB_WORKBENCH_HOME pointing at a fresh temporary
// directory, so a test can never read or damage the configuration, projects or
// database of the user running the suite.
#pragma once

#include <QString>
#include <QTemporaryDir>

namespace eda {

class Application;

class TestEnvironment
{
public:
    // Creates the temporary home for this test run. Must be called before the
    // Application object is created.
    static void init(const QString &testName);

    // Creates and initialises the Application singleton.
    static Application *createApplication();

    static void cleanup();
    static QString root();
    static QString configDir();
    static QString dataDir();

private:
    static QTemporaryDir *m_dir;
    static Application *m_application;
};

} // namespace eda
