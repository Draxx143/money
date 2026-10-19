// Unit tests for the configurable application identity.
//
// The product name must never be hardcoded: it is configuration, and the user is
// allowed to change it. These tests pin that behaviour down, including the
// requirement that no shipped default is branded "CIRCUIT AI".
#include <QtTest>

#include "../../src/application/AppIdentity.h"
#include "../../src/application/Settings.h"
#include "../TestEnvironment.h"

class TestAppIdentity : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("identity")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void defaultIdentityIsSane()
    {
        const eda::Identity identity;
        QVERIFY(!identity.name.trimmed().isEmpty());
        QVERIFY(identity.accent.isValid());
        QVERIFY(identity.accentSecondary.isValid());
        QCOMPARE(identity.displayName(), QStringLiteral("PCB Workbench"));
        QVERIFY2(!identity.displayName().contains(QStringLiteral("CIRCUIT"), Qt::CaseInsensitive),
                 "the default product name must not be CIRCUIT AI");
    }

    void blankNameFallsBackToAPlaceholder()
    {
        eda::AppIdentity identity;
        eda::Identity blank = identity.identity();
        blank.name = QStringLiteral("   ");
        identity.setIdentity(blank);
        QCOMPARE(identity.name(), QStringLiteral("PCB Workbench"));
    }

    void nameIsConfigurableAndPersistent()
    {
        const QString file = eda::TestEnvironment::root() + QStringLiteral("/identity.conf");

        eda::Settings settings;
        settings.setFileName(file);
        eda::AppIdentity identity;
        identity.load(&settings);
        QCOMPARE(identity.name(), QStringLiteral("PCB Workbench"));

        eda::Identity changed = identity.identity();
        changed.name = QStringLiteral("Bench Of Theseus");
        changed.organization = QStringLiteral("Example Repair Lab");
        changed.accent = QColor(QStringLiteral("#4DA3FF"));
        identity.setIdentity(changed);
        identity.save(&settings);
        QVERIFY(settings.save());

        eda::Settings reloaded;
        reloaded.setFileName(file);
        QVERIFY(reloaded.load());
        eda::AppIdentity identity2;
        identity2.load(&reloaded);
        QCOMPARE(identity2.name(), QStringLiteral("Bench Of Theseus"));
        QCOMPARE(identity2.identity().organization, QStringLiteral("Example Repair Lab"));
        QCOMPARE(identity2.identity().accent, QColor(QStringLiteral("#4DA3FF")));
    }

    void windowTitleCombinesDocumentAndApplication()
    {
        eda::AppIdentity identity;
        eda::Identity id = identity.identity();
        id.name = QStringLiteral("Bench");
        identity.setIdentity(id);

        QCOMPARE(identity.windowTitle(QStringLiteral("motor")), QStringLiteral("motor - Bench"));
        QCOMPARE(identity.windowTitle(), QStringLiteral("Bench"));
        QCOMPARE(identity.windowTitle(QStringLiteral("   ")), QStringLiteral("Bench"));
    }

    void iconIsGeneratedWithoutArtworkFiles()
    {
        eda::AppIdentity identity;
        QVERIFY(!identity.icon().isNull());

        // A path that does not exist must fall back to the generated icon
        // instead of showing an empty one.
        eda::Identity broken = identity.identity();
        broken.iconPath = QStringLiteral("/nonexistent/icon.png");
        identity.setIdentity(broken);
        QVERIFY(!identity.icon().isNull());
    }

    void changingTheIdentityNotifiesListeners()
    {
        eda::AppIdentity identity;
        int changes = 0;
        QObject::connect(&identity, &eda::AppIdentity::changed, this,
                         [&changes]() { ++changes; });

        eda::Identity unchanged = identity.identity();
        identity.setIdentity(unchanged);
        QCOMPARE(changes, 0);

        eda::Identity changed = identity.identity();
        changed.accent = QColor(QStringLiteral("#4DA3FF"));
        identity.setIdentity(changed);
        QCOMPARE(changes, 1);
    }
};

QTEST_MAIN(TestAppIdentity)
#include "test_appidentity.moc"
