#include "TestEnvironment.h"

#include "../src/application/Application.h"
#include "../src/core/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace eda {

QTemporaryDir *TestEnvironment::m_dir = nullptr;
Application *TestEnvironment::m_application = nullptr;

void TestEnvironment::init(const QString &testName)
{
    cleanup();
    m_dir = new QTemporaryDir(QDir::temp().absoluteFilePath(
        QStringLiteral("pcbwb-%1-XXXXXX").arg(testName)));
    if (!m_dir->isValid())
        qFatal("cannot create the test directory");
    qputenv("PCB_WORKBENCH_HOME", m_dir->path().toLocal8Bit());
    QDir().mkpath(root());
}

Application *TestEnvironment::createApplication()
{
    if (m_application)
        return m_application;
    m_application = new Application();
    QString error;
    if (!m_application->initialize(&error))
        qFatal("cannot initialise the application: %s", qPrintable(error));
    return m_application;
}

void TestEnvironment::cleanup()
{
    delete m_application;
    m_application = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

QString TestEnvironment::root() { return m_dir ? m_dir->path() : QString(); }
QString TestEnvironment::configDir() { return Paths::configDir(); }
QString TestEnvironment::dataDir() { return Paths::dataDir(); }

} // namespace eda
