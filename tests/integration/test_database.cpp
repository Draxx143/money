// Integration tests for the local knowledge database.
//
// These run against a real SQLite file in the test's own data directory, with
// the real FTS5 index: the search behaviour the user gets is the behaviour that
// is tested here, not a mock of it.
//
// QTEST_MAIN (not QTEST_APPLESS_MAIN) because Qt's SQL layer needs a
// QCoreApplication instance to exist.
#include <QtTest>

#include "../../src/core/Database.h"
#include "../../src/core/Paths.h"
#include "../../src/core/ProjectModel.h"
#include "../TestEnvironment.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>

class TestDatabase : public QObject
{
    Q_OBJECT

private:
    QString m_path;

    QString databasePath(const QString &name) const
    {
        return QDir::cleanPath(QStringLiteral("%1/%2.sqlite3")
                                   .arg(eda::TestEnvironment::root(), name));
    }

    // Stores a copy, because the database fills in the id of the fact it stored.
    qlonglong addFactTo(eda::Database &database, const eda::Fact &fact)
    {
        eda::Fact copy = fact;
        if (!database.addFact(&copy))
            return 0;
        return copy.id;
    }

    eda::Fact makeFact(const QString &statement, const QString &topic = QString(),
                       const QString &componentUid = QString()) const
    {
        eda::Fact fact;
        fact.componentUid = componentUid;
        fact.topic = topic;
        fact.statement = statement;
        fact.provenance.title = QStringLiteral("ATtiny85 datasheet");
        fact.provenance.publisher = QStringLiteral("Microchip");
        fact.provenance.location = QStringLiteral("page 164");
        fact.provenance.status = eda::VerificationStatus::OfficialSourceVerified;
        fact.provenance.valueSource = eda::ValueSource::ExtractedFromSource;
        fact.provenance.retrieved = QDateTime::currentDateTimeUtc();
        return fact;
    }

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("database")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void init()
    {
        m_path = databasePath(QStringLiteral("knowledge"));
        QFile::remove(m_path);
    }

    void aNewDatabaseIsCreatedEmptyAndReady()
    {
        eda::Database database;
        QVERIFY2(database.open(m_path), qPrintable(database.lastError()));
        QVERIFY(database.isOpen());
        QCOMPARE(database.path(), m_path);
        QCOMPARE(database.version(), eda::Database::schemaVersion());
        QVERIFY(database.isEmpty());
        QCOMPARE(database.componentCount(), 0);
        QCOMPARE(database.factCount(), 0);
        QVERIFY(QFileInfo::exists(m_path));
        QVERIFY2(database.checkIntegrity(), qPrintable(database.lastError()));
    }

    void aFactIsStoredWithEverythingItCameFrom()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));

        eda::Fact fact = makeFact(QStringLiteral("Absolute maximum supply voltage is 6.0 V"),
                                  QStringLiteral("absolute-maximum"));
        QVERIFY2(database.addFact(&fact), qPrintable(database.lastError()));
        QVERIFY(fact.id > 0);
        QCOMPARE(database.factCount(), 1);

        eda::Fact stored = database.factsForComponent(QString()).first();
        QCOMPARE(stored.statement, fact.statement);
        QCOMPARE(stored.topic, QStringLiteral("absolute-maximum"));
        QCOMPARE(stored.provenance.publisher, QStringLiteral("Microchip"));
        QCOMPARE(stored.provenance.location, QStringLiteral("page 164"));
        QCOMPARE(eda::toString(stored.provenance.status),
                 eda::toString(eda::VerificationStatus::OfficialSourceVerified));
        QCOMPARE(eda::toString(stored.provenance.valueSource),
                 eda::toString(eda::ValueSource::ExtractedFromSource));
        QVERIFY(stored.provenance.retrieved.isValid());
    }

    void aFactWithoutAStatementIsRefused()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));
        eda::Fact empty = makeFact(QStringLiteral("   "));
        QVERIFY(!database.addFact(&empty));
        QVERIFY2(!database.lastError().isEmpty(), "the reason has to be reported");
        QCOMPARE(database.factCount(), 0);
    }

    void fullTextSearchFindsWordsPrefixesAndPhrases()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));
        addFactTo(database, makeFact(QStringLiteral("Absolute maximum supply voltage is 6.0 V"),
                                   QStringLiteral("absolute-maximum")));
        addFactTo(database, makeFact(QStringLiteral("Brown-out detection triggers at 2.7 V"),
                                   QStringLiteral("typical-value")));
        addFactTo(database, makeFact(QStringLiteral("Pin 4 is ground; pin 8 is VCC"),
                                   QStringLiteral("pinout")));

        // A whole word.
        QCOMPARE(database.searchFacts(QStringLiteral("brown-out")).size(), 1);
        QCOMPARE(database.lastSearchMode(), QStringLiteral("fts5"));
        // A prefix: the user is still typing.
        QCOMPARE(database.searchFacts(QStringLiteral("absolut")).size(), 1);
        // Two words have to appear together.
        QCOMPARE(database.searchFacts(QStringLiteral("supply voltage")).size(), 1);
        QCOMPARE(database.searchFacts(QStringLiteral("voltage ground")).size(), 0);
        // The topic is searched too.
        QCOMPARE(database.searchFacts(QStringLiteral("pinout")).size(), 1);
        // Nothing matches, and nothing is invented.
        QVERIFY(database.searchFacts(QStringLiteral("quantum flux")).isEmpty());

        // An empty search lists what was stored last, newest first.
        const QVector<eda::Fact> recent = database.searchFacts(QString());
        QCOMPARE(recent.size(), 3);
        QCOMPARE(database.lastSearchMode(), QStringLiteral("recent"));
        QCOMPARE(recent.first().topic, QStringLiteral("pinout"));
    }

    void userInputCannotBreakTheSearch()
    {
        // Everything a user might type into a search box, including things that
        // mean something to the query language.
        const QStringList hostile = {
            QStringLiteral("\" OR 1=1 --"),
            QStringLiteral("AND OR NOT"),
            QStringLiteral("*("),
            QStringLiteral("drop table facts;"),
            QStringLiteral("((((("),
            QStringLiteral("a\"b"),
            QStringLiteral("NEAR(a b)"),
            QStringLiteral("^#$%^"),
            QString()
        };
        eda::Database database;
        QVERIFY(database.open(m_path));
        addFactTo(database, makeFact(QStringLiteral("Supply voltage range 2.7 V to 5.5 V")));

        for (const QString &text : hostile) {
            const QVector<eda::Fact> results = database.searchFacts(text);
            QVERIFY2(true, text.toUtf8().constData());
            // The database must survive the query and stay usable.
            QVERIFY2(database.checkIntegrity(), qPrintable(text));
            Q_UNUSED(results)
        }
        // And a normal search still works afterwards.
        QCOMPARE(database.searchFacts(QStringLiteral("voltage")).size(), 1);
    }

    void theQueryBuilderQuotesWhatTheUserTypes()
    {
        QCOMPARE(eda::Database::toMatchQuery(QStringLiteral("brown out")),
                 QStringLiteral("\"brown\" AND \"out\"*"));
        QCOMPARE(eda::Database::toMatchQuery(QStringLiteral("  ")), QString());
        // A quote in the input is a separator like any other: it never reaches
        // the query.
        QCOMPARE(eda::Database::toMatchQuery(QStringLiteral("a\"b")),
                 QStringLiteral("\"a\" AND \"b\"*"));
        QCOMPARE(eda::Database::toMatchQuery(QStringLiteral("TPS54331")),
                 QStringLiteral("\"TPS54331\"*"));
    }

    void factsBelongToAComponentAndCanBeRemoved()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));
        addFactTo(database, makeFact(QStringLiteral("Pin 1 is the cathode"),
                                   QStringLiteral("pinout"), QStringLiteral("c-1")));
        addFactTo(database, makeFact(QStringLiteral("General note about decoupling"),
                                   QStringLiteral("board-note")));

        QCOMPARE(database.factsForComponent(QStringLiteral("c-1")).size(), 1);
        QVERIFY(database.factsForComponent(QStringLiteral("c-2")).isEmpty());
        QCOMPARE(database.factCount(), 2);

        const QVector<eda::Fact> facts = database.factsForComponent(QStringLiteral("c-1"));
        QVERIFY(database.removeFact(facts.first().id));
        QCOMPARE(database.factCount(), 1);
        // The index follows the table: a removed fact is not found any more.
        QVERIFY(database.searchFacts(QStringLiteral("cathode")).isEmpty());
        QVERIFY2(database.checkIntegrity(), qPrintable(database.lastError()));
    }

    void componentRecordsAreIdentifiedByPartNumber()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));

        eda::ComponentRecord record;
        record.mpn = QStringLiteral("TPS54331DDA");
        record.manufacturer = QStringLiteral("Texas Instruments");
        record.description = QStringLiteral("3 A step-down converter");
        record.category = eda::ComponentCategory::IntegratedCircuit;
        record.provenance.status = eda::VerificationStatus::ExtractedFromSource;
        record.provenance.localPath = QStringLiteral("/boards/demo.kicad_pcb");
        QVERIFY2(database.upsertComponent(&record), qPrintable(database.lastError()));
        QVERIFY(!record.uid.isEmpty());
        QVERIFY(record.created.isValid());

        // The same part from another board updates the record; it is not a
        // second copy.
        eda::ComponentRecord again = record;
        again.uid.clear();
        again.description = QStringLiteral("3 A, 28 V step-down converter");
        QVERIFY(database.upsertComponent(&again));
        QCOMPARE(again.uid, record.uid);
        QCOMPARE(database.componentCount(), 1);
        QCOMPARE(database.componentByUid(record.uid).description, again.description);

        // A different manufacturer is a different part.
        eda::ComponentRecord other;
        other.mpn = QStringLiteral("TPS54331DDA");
        other.manufacturer = QStringLiteral("Unknown");
        QVERIFY(database.upsertComponent(&other));
        QCOMPARE(database.componentCount(), 2);

        QCOMPARE(database.findComponents(QStringLiteral("step-down")).size(), 1);
        QCOMPARE(database.findComponents(QStringLiteral("TPS54331")).size(), 2);
        QVERIFY(database.findComponents(QStringLiteral("no such part")).isEmpty());
        QCOMPARE(eda::toString(database.componentByUid(record.uid).category),
                 eda::toString(eda::ComponentCategory::IntegratedCircuit));
        QCOMPARE(database.componentByUid(record.uid).provenance.localPath,
                 QStringLiteral("/boards/demo.kicad_pcb"));
    }

    void removingAComponentTakesItsFactsWithIt()
    {
        eda::Database database;
        QVERIFY(database.open(m_path));
        eda::ComponentRecord record;
        record.mpn = QStringLiteral("STM32F103C8T6");
        record.manufacturer = QStringLiteral("STMicroelectronics");
        QVERIFY(database.upsertComponent(&record));
        addFactTo(database, makeFact(QStringLiteral("Boot0 must be low to run the flash loader"),
                                   QStringLiteral("board-note"), record.uid));
        addFactTo(database, makeFact(QStringLiteral("Decouple every VDD pin with 100 nF"),
                                   QStringLiteral("board-note")));
        QCOMPARE(database.factCount(), 2);

        QVERIFY(database.removeComponent(record.uid));
        QCOMPARE(database.componentCount(), 0);
        // Only the fact that belonged to the component is gone.
        QCOMPARE(database.factCount(), 1);
        QVERIFY(database.searchFacts(QStringLiteral("boot0")).isEmpty());
        QCOMPARE(database.searchFacts(QStringLiteral("decouple")).size(), 1);
    }

    void dataSurvivesClosingAndReopening()
    {
        qlonglong id = 0;
        QString uid;
        {
            eda::Database database;
            QVERIFY(database.open(m_path));
            eda::ComponentRecord record;
            record.mpn = QStringLiteral("AMS1117-3.3");
            record.manufacturer = QStringLiteral("Advanced Monolithic Systems");
            QVERIFY(database.upsertComponent(&record));
            uid = record.uid;
            eda::Fact fact = makeFact(QStringLiteral("Dropout voltage is 1.1 V at 1 A"),
                                      QStringLiteral("typical-value"), uid);
            QVERIFY(database.addFact(&fact));
            id = fact.id;
        }
        {
            eda::Database reopened;
            QVERIFY2(reopened.open(m_path), qPrintable(reopened.lastError()));
            QCOMPARE(reopened.version(), eda::Database::schemaVersion());
            QCOMPARE(reopened.componentCount(), 1);
            QCOMPARE(reopened.factCount(), 1);
            QCOMPARE(reopened.factsForComponent(uid).first().id, id);
            QCOMPARE(reopened.searchFacts(QStringLiteral("dropout")).size(), 1);
            QCOMPARE(reopened.topics(), QStringList({ QStringLiteral("typical-value") }));
            QVERIFY2(reopened.checkIntegrity(), qPrintable(reopened.lastError()));
        }
    }

    void aDamagedFileIsRefusedAndLeftAlone()
    {
        QFile file(m_path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is not a database, it is a very small file of text\n");
        file.close();
        const qint64 sizeBefore = QFileInfo(m_path).size();

        eda::Database database;
        QString error;
        QVERIFY(!database.open(m_path, &error));
        QVERIFY2(!error.isEmpty(), "the reason has to be reported");
        QVERIFY(!database.isOpen());
        // The file is not deleted and not overwritten: it may be the only copy.
        QCOMPARE(QFileInfo(m_path).size(), sizeBefore);
    }

    void aDatabaseFromANewerVersionIsRefused()
    {
        {
            eda::Database database;
            QVERIFY(database.open(m_path));
        }
        {
            // Pretend a newer application wrote it.
            QSqlDatabase connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                               QStringLiteral("test-sabotage"));
            connection.setDatabaseName(m_path);
            QVERIFY(connection.open());
            QSqlQuery query(connection);
            QVERIFY(query.exec(QStringLiteral("UPDATE meta SET value = '99' "
                                              "WHERE key = 'schema_version'")));
            connection.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("test-sabotage"));

        eda::Database database;
        QString error;
        QVERIFY(!database.open(m_path, &error));
        QVERIFY2(error.contains(QStringLiteral("newer version")), qPrintable(error));
    }

    void theDatabaseLivesInTheUserDataDirectory()
    {
        QVERIFY(eda::Database::defaultPath().startsWith(eda::Paths::dataDir()));
        QVERIFY(eda::Database::defaultPath().endsWith(QStringLiteral(".sqlite3")));
    }
};

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
