// Unit tests for the logging subsystem.
#include <QtTest>

#include "../../src/core/Logging.h"
#include "../../src/core/Paths.h"
#include "../TestEnvironment.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

class TestLogging : public QObject
{
    Q_OBJECT

private:
    // A slot-local counter captured by reference would outlive the test
    // function that owns it as soon as the next log entry is emitted, so the
    // signal counter lives here.
    int m_entrySignals = 0;

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("logging")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void levelsAreOrdered()
    {
        QVERIFY(eda::Logger::Debug < eda::Logger::Info);
        QVERIFY(eda::Logger::Info < eda::Logger::Warning);
        QVERIFY(eda::Logger::Warning < eda::Logger::Error);
        QVERIFY(eda::Logger::Error < eda::Logger::Critical);
    }

    void levelNamesRoundTrip()
    {
        QCOMPARE(eda::logLevelName(eda::Logger::Debug), QStringLiteral("DBG"));
        QCOMPARE(eda::logLevelName(eda::Logger::Critical), QStringLiteral("CRT"));
        QCOMPARE(eda::logLevelFromName(QStringLiteral("warning")), eda::Logger::Warning);
        QCOMPARE(eda::logLevelFromName(QStringLiteral("debug")), eda::Logger::Debug);
        QCOMPARE(eda::logLevelFromName(QStringLiteral("off")), eda::Logger::Critical);
        QCOMPARE(eda::logLevelFromName(QStringLiteral("nonsense")), eda::Logger::Info); // unknown -> Info
        for (int level = eda::Logger::Debug; level <= eda::Logger::Critical; ++level) {
            const auto casted = static_cast<eda::Logger::Level>(level);
            QCOMPARE(eda::logLevelFromName(eda::logLevelName(casted)), casted);
        }
    }

    void entriesReachHistoryAndSignal()
    {
        auto &logger = eda::Logger::instance();
        logger.setMinimumLevel(eda::Logger::Debug);
        logger.setMaxHistoryEntries(200);

        m_entrySignals = 0;
        QObject::connect(&logger, &eda::Logger::entryAdded, this,
                         [this](const eda::Logger::Entry &) { ++m_entrySignals; });

        const int before = logger.history().size();
        eda::logInfo(QStringLiteral("unit"), QStringLiteral("history test entry"));
        QCOMPARE(m_entrySignals, 1);
        QCOMPARE(logger.history().size(), before + 1);

        const eda::Logger::Entry entry = logger.history().last();
        QCOMPARE(entry.level, eda::Logger::Info);
        QCOMPARE(entry.category, QStringLiteral("unit"));
        QCOMPARE(entry.message, QStringLiteral("history test entry"));
        QVERIFY(entry.timestamp.isValid());
    }

    void minimumLevelFilters()
    {
        auto &logger = eda::Logger::instance();
        logger.setMinimumLevel(eda::Logger::Warning);
        const int before = logger.history().size();
        eda::logDebug(QStringLiteral("unit"), QStringLiteral("dropped"));
        eda::logInfo(QStringLiteral("unit"), QStringLiteral("dropped"));
        QCOMPARE(logger.history().size(), before);
        eda::logWarning(QStringLiteral("unit"), QStringLiteral("kept"));
        QCOMPARE(logger.history().size(), before + 1);
        logger.setMinimumLevel(eda::Logger::Debug);
    }

    void historyIsBounded()
    {
        auto &logger = eda::Logger::instance();
        logger.setMinimumLevel(eda::Logger::Debug);
        logger.setMaxHistoryEntries(120);
        for (int i = 0; i < 400; ++i)
            eda::logDebug(QStringLiteral("unit"), QString::number(i));
        QVERIFY(logger.history().size() <= 120);
        logger.setMaxHistoryEntries(5000);
    }

    void writesToLogFile()
    {
        const QString directory = eda::TestEnvironment::root() + QStringLiteral("/logs");
        auto &logger = eda::Logger::instance();
        logger.setLogDirectory(directory);
        logger.setMinimumLevel(eda::Logger::Debug);

        const QString marker = QStringLiteral("file-marker-%1").arg(QDateTime::currentMSecsSinceEpoch());
        eda::logError(QStringLiteral("unit"), marker);

        QFile file(logger.currentLogFile());
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.contains(marker));
        QVERIFY(content.contains(QStringLiteral("ERR")));

        logger.setLogDirectory(eda::Paths::logDir());
    }
};

QTEST_APPLESS_MAIN(TestLogging)
#include "test_logging.moc"
