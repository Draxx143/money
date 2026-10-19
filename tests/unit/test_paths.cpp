// Unit tests for the XDG path helper.
//
// These tests prove that the application never writes to a hardcoded location:
// with PCB_WORKBENCH_HOME redirected the whole tree must follow.
#include <QtTest>

#include "../../src/core/Paths.h"
#include "../TestEnvironment.h"

#include <QDir>

class TestPaths : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("paths")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void everyPathIsUnderTheConfiguredHome()
    {
        const QString home = QDir(eda::TestEnvironment::root()).canonicalPath();
        const QStringList paths = { eda::Paths::configDir(),    eda::Paths::dataDir(),
                                    eda::Paths::cacheDir(),     eda::Paths::stateDir(),
                                    eda::Paths::logDir(),       eda::Paths::backupDir(),
                                    eda::Paths::documentDir(),  eda::Paths::defaultProjectDir(),
                                    eda::Paths::settingsFile(), eda::Paths::databaseFile(),
                                    eda::Paths::workspaceStateFile() };
        for (const QString &path : paths) {
            QVERIFY2(!path.isEmpty(), "a path must never be empty");
            QVERIFY2(path.startsWith(home), qPrintable(path + QStringLiteral(" escapes ") + home));
        }
    }

    void directoriesAreCreatedOnDemand()
    {
        const QString config = eda::Paths::configDir();
        QDir().remove(config);
        QVERIFY(!QDir(config).exists());
        QVERIFY(eda::Paths::ensureDirectories());
        QVERIFY(QDir(config).exists());
        QVERIFY(QDir(eda::Paths::dataDir()).exists());
        QVERIFY(QDir(eda::Paths::cacheDir()).exists());
        QVERIFY(QDir(eda::Paths::stateDir()).exists());
    }

    void filesHaveTheExpectedNames()
    {
        QVERIFY(eda::Paths::settingsFile().endsWith(QStringLiteral("settings.conf")));
        QVERIFY(eda::Paths::databaseFile().endsWith(QStringLiteral(".sqlite3")));
        QVERIFY(eda::Paths::workspaceStateFile().endsWith(QStringLiteral("workspace.ini")));
        QVERIFY(eda::Paths::logDir().endsWith(QStringLiteral("logs")));
    }

    void directoriesAreDistinct()
    {
        QVERIFY(eda::Paths::configDir() != eda::Paths::dataDir());
        QVERIFY(eda::Paths::dataDir() != eda::Paths::cacheDir());
        QVERIFY(eda::Paths::cacheDir() != eda::Paths::stateDir());
        QVERIFY(eda::Paths::settingsFile() != eda::Paths::databaseFile());
    }
};

QTEST_APPLESS_MAIN(TestPaths)
#include "test_paths.moc"
