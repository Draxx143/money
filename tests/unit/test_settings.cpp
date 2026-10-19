// Unit tests for the configuration store.
#include <QtTest>

#include "../../src/application/Settings.h"
#include "../TestEnvironment.h"

#include <QDir>
#include <QFileInfo>

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("settings")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void emptyStoreReturnsDefaults()
    {
        eda::Settings settings;
        QCOMPARE(settings.value(QStringLiteral("does/not/exist"), 42).toInt(), 42);
        QVERIFY(!settings.contains(QStringLiteral("does/not/exist")));
    }

    void roundTripsThroughDisk()
    {
        const QString file = eda::TestEnvironment::root() + QStringLiteral("/settings.conf");

        {
            eda::Settings settings;
            settings.setFileName(file);
            QVERIFY(settings.save());
            settings.setValue(QStringLiteral("appearance/theme"), QStringLiteral("dark"));
            settings.setValue(QStringLiteral("ai/timeoutSeconds"), 90);
            settings.setValue(QStringLiteral("ai/enabled"), true);
            QVERIFY(settings.save());
        }
        QVERIFY(QFileInfo::exists(file));

        eda::Settings reloaded;
        reloaded.setFileName(file);
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.value(QStringLiteral("appearance/theme")).toString(), QStringLiteral("dark"));
        QCOMPARE(reloaded.value(QStringLiteral("ai/timeoutSeconds")).toInt(), 90);
        QCOMPARE(reloaded.value(QStringLiteral("ai/enabled")).toBool(), true);
    }

    void resetRemovesValues()
    {
        const QString file = eda::TestEnvironment::root() + QStringLiteral("/reset.conf");
        eda::Settings settings;
        settings.setFileName(file);
        settings.setValue(QStringLiteral("a/b"), 1);
        QVERIFY(settings.save());
        settings.reset();
        QVERIFY(settings.save());

        eda::Settings reloaded;
        reloaded.setFileName(file);
        QVERIFY(reloaded.load());
        QVERIFY(!reloaded.contains(QStringLiteral("a/b")));
    }

    void recentProjectsAreBoundedAndUnique()
    {
        eda::Settings settings;
        settings.setFileName(eda::TestEnvironment::root() + QStringLiteral("/recent.conf"));

        settings.addRecentProject(QStringLiteral("/tmp/a.pwb"));
        settings.addRecentProject(QStringLiteral("/tmp/b.pwb"));
        settings.addRecentProject(QStringLiteral("/tmp/a.pwb"));
        QCOMPARE(settings.recentProjects(), QStringList({ QStringLiteral("/tmp/a.pwb"),
                                                          QStringLiteral("/tmp/b.pwb") }));

        for (int i = 0; i < 20; ++i)
            settings.addRecentProject(QStringLiteral("/tmp/p%1.pwb").arg(i));
        QCOMPARE(settings.recentProjects().size(), 10);
        QCOMPARE(settings.recentProjects().first(), QStringLiteral("/tmp/p19.pwb"));

        settings.clearRecentProjects();
        QVERIFY(settings.recentProjects().isEmpty());
    }

    void writableInsideTemporaryHome()
    {
        eda::Settings settings;
        QVERIFY(settings.isWritable());
    }
};

QTEST_APPLESS_MAIN(TestSettings)
#include "test_settings.moc"
