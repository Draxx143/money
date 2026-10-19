// Integration tests for the project document: create, save, reopen, error
// handling and crash recovery.
#include <QtTest>

#include "../../src/core/Paths.h"
#include "../../src/core/Project.h"
#include "../TestEnvironment.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>

class TestProject : public QObject
{
    Q_OBJECT

private:
    QString m_dir;

    QString writeFile(const QString &name, const QByteArray &content)
    {
        const QString path = m_dir + QLatin1Char('/') + name;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return QString();
        file.write(content);
        file.close();
        return path;
    }

private slots:
    void initTestCase()
    {
        eda::TestEnvironment::init(QStringLiteral("project"));
        m_dir = eda::TestEnvironment::root() + QStringLiteral("/projects");
        QDir().mkpath(m_dir);
    }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void createSaveAndReopen()
    {
        eda::Project project;
        QString error;

        QVERIFY(project.create(QStringLiteral("Motor controller"), m_dir, &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(project.isOpen());
        QVERIFY(!project.isDirty());
        QVERIFY(QFileInfo::exists(project.filePath()));
        QCOMPARE(project.displayName(), QStringLiteral("Motor controller"));

        // Add something to the model and save.
        eda::Board board;
        board.uid = QStringLiteral("b-1");
        board.name = QStringLiteral("controller rev C");
        board.sourceFormat = QStringLiteral("kicad-pcb");
        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("U1");
        component.value = QStringLiteral("IR2104");
        board.components.append(component);
        project.data().boards.append(board);
        project.markDirty();
        QVERIFY(project.isDirty());
        QVERIFY(project.save(&error));
        QVERIFY(!project.isDirty());

        // Reopen in a second document.
        eda::Project reopened;
        QVERIFY(reopened.open(project.filePath(), &error));
        QCOMPARE(reopened.displayName(), QStringLiteral("Motor controller"));
        QCOMPARE(reopened.data().boards.size(), 1);
        QCOMPARE(reopened.data().boards.first().components.size(), 1);
        QCOMPARE(reopened.data().boards.first().components.first().reference, QStringLiteral("U1"));
        QCOMPARE(reopened.primaryBoard()->name, QStringLiteral("controller rev C"));
    }

    void saveAsWritesANewFile()
    {
        eda::Project project;
        QString error;
        QVERIFY(project.create(QStringLiteral("Save as"), m_dir, &error));

        const QString other = m_dir + QStringLiteral("/other-name.pwb");
        QVERIFY(project.saveAs(other, &error));
        QVERIFY(QFileInfo::exists(other));
        QCOMPARE(project.filePath(), QFileInfo(other).absoluteFilePath());

        eda::Project reopened;
        QVERIFY(reopened.open(other, &error));
        QCOMPARE(reopened.displayName(), QStringLiteral("Save as"));
    }

    void closedProjectRefusesSave()
    {
        eda::Project project;
        QString error;
        QVERIFY(!project.save(&error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!project.isDirty());
    }

    void malformedFileIsRejected()
    {
        const QString path = writeFile(QStringLiteral("broken.pwb"), QByteArray("{ this is not json"));
        eda::Project project;
        QString error;
        QVERIFY(!project.open(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!project.isOpen());
    }

    void foreignFileIsRejected()
    {
        QJsonObject other;
        other.insert(QStringLiteral("format"), QStringLiteral("some-other-tool"));
        const QString path = writeFile(QStringLiteral("foreign.pwb"),
                                       QJsonDocument(other).toJson());
        eda::Project project;
        QString error;
        QVERIFY(!project.open(path, &error));
        QVERIFY2(error.contains(QStringLiteral("not a project file")), qPrintable(error));
    }

    void futureFormatVersionIsRejected()
    {
        QJsonObject root;
        root.insert(QStringLiteral("format"), QStringLiteral("pcb-workbench-project"));
        root.insert(QStringLiteral("formatVersion"), eda::Project::schemaVersion() + 5);
        root.insert(QStringLiteral("project"), QJsonObject());
        const QString path = writeFile(QStringLiteral("future.pwb"), QJsonDocument(root).toJson());

        eda::Project project;
        QString error;
        QVERIFY(!project.open(path, &error));
        QVERIFY2(error.contains(QStringLiteral("newer version")), qPrintable(error));
    }

    void missingFileIsReported()
    {
        eda::Project project;
        QString error;
        QVERIFY(!project.open(m_dir + QStringLiteral("/does-not-exist.pwb"), &error));
        QVERIFY(error.contains(QStringLiteral("cannot be opened")));
    }

    void recoveryDataRoundTrip()
    {
        eda::Project project;
        QString error;
        QVERIFY(project.create(QStringLiteral("Recovery"), m_dir, &error));
        QVERIFY(!project.hasRecoveryData());

        eda::Board board;
        board.uid = QStringLiteral("b-rec");
        board.name = QStringLiteral("unsaved board");
        project.data().boards.append(board);
        project.markDirty();

        QVERIFY(project.close(&error));
        QVERIFY(project.hasRecoveryData());

        QVERIFY(project.restoreFromRecovery(&error));
        QVERIFY(project.isOpen());
        QVERIFY(project.isDirty());
        QCOMPARE(project.data().boards.first().name, QStringLiteral("unsaved board"));

        project.discardRecoveryData();
        QVERIFY(!project.hasRecoveryData());
    }

    void openingAnotherProjectClosesTheFirst()
    {
        eda::Project project;
        QString error;
        QVERIFY(project.create(QStringLiteral("First"), m_dir, &error));
        const QString first = project.filePath();
        QVERIFY(project.create(QStringLiteral("Second"), m_dir, &error));
        QVERIFY(project.filePath() != first);
        QCOMPARE(project.displayName(), QStringLiteral("Second"));
    }
};

QTEST_APPLESS_MAIN(TestProject)
#include "test_project.moc"
