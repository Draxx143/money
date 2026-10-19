// Integration tests for test points and measurements.
//
// The verdict "pass" or "fail" is the one claim a diagnostic tool must never get
// wrong, so these tests are about the edges: a measurement without an
// expectation, a value in the wrong unit, a tolerance given in percent, and the
// difference between "not tested" and "failed".
#include <QtTest>

#include "../../src/core/Measurements.h"
#include "../../src/core/Project.h"
#include "../TestEnvironment.h"

#include <QDir>

class TestMeasurements : public QObject
{
    Q_OBJECT

private:
    eda::TestPoint point(double expected, const QString &unit, double tolerance = 0.0,
                         bool percent = false) const
    {
        eda::TestPoint testPoint;
        testPoint.uid = QStringLiteral("tp-%1").arg(expected);
        testPoint.identifier = QStringLiteral("TP%1").arg(expected);
        testPoint.category = QStringLiteral("voltage");
        testPoint.expectedValue = expected;
        testPoint.hasExpectedValue = true;
        testPoint.tolerance = tolerance;
        testPoint.hasTolerance = tolerance > 0.0;
        testPoint.toleranceIsPercent = percent;
        testPoint.unit = unit;
        testPoint.provenance.status = eda::VerificationStatus::OfficialSourceVerified;
        testPoint.provenance.valueSource = eda::ValueSource::ExtractedFromSource;
        return testPoint;
    }

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("measurements")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void aValueInsideTheTolerancePasses()
    {
        const eda::TestPoint testPoint = point(5.0, QStringLiteral("V"), 0.25);
        QString explanation;
        const eda::Measurement measurement = eda::recordMeasurement(testPoint, 4.87,
                                                                    QStringLiteral("V"));
        QCOMPARE(eda::toString(measurement.status), eda::toString(eda::MeasurementStatus::Pass));
        QCOMPARE(eda::toString(eda::evaluateMeasurement(measurement, &explanation)),
                 eda::toString(eda::MeasurementStatus::Pass));
        QVERIFY2(explanation.contains(QStringLiteral("inside the limits")),
                 qPrintable(explanation));
        // The expectation is copied into the measurement: changing the test
        // point later must not rewrite what was measured.
        QCOMPARE(measurement.expectedValue, 5.0);
        QCOMPARE(measurement.tolerance, 0.25);
        QVERIFY(measurement.hasTolerance);
        QCOMPARE(eda::toString(measurement.source), eda::toString(eda::ValueSource::Measured));
        QVERIFY(!measurement.uid.isEmpty());
        QVERIFY(measurement.timestamp.isValid());
    }

    void aValueOutsideTheToleranceFails()
    {
        const eda::TestPoint testPoint = point(5.0, QStringLiteral("V"), 0.25);
        const eda::Measurement measurement = eda::recordMeasurement(testPoint, 4.5,
                                                                    QStringLiteral("V"));
        QCOMPARE(eda::toString(measurement.status), eda::toString(eda::MeasurementStatus::Fail));
        QVERIFY2(eda::measurementExplanation(measurement).contains(
                     QStringLiteral("outside the limits")),
                 qPrintable(eda::measurementExplanation(measurement)));
    }

    void theEdgeOfTheToleranceIsAPass()
    {
        const eda::TestPoint testPoint = point(5.0, QStringLiteral("V"), 0.25);
        const eda::Measurement inside = eda::recordMeasurement(testPoint, 5.25, QStringLiteral("V"));
        QCOMPARE(eda::toString(inside.status), eda::toString(eda::MeasurementStatus::Pass));
        const eda::Measurement outside = eda::recordMeasurement(testPoint, 5.26, QStringLiteral("V"));
        QCOMPARE(eda::toString(outside.status), eda::toString(eda::MeasurementStatus::Fail));
    }

    void aToleranceCanBeGivenInPercent()
    {
        eda::TestPoint testPoint = point(100.0, QStringLiteral("kohm"));
        testPoint.tolerance = 5.0;
        testPoint.hasTolerance = true;
        testPoint.toleranceIsPercent = true;

        const eda::Tolerance tolerance = eda::toleranceOf(testPoint);
        QCOMPARE(tolerance.absoluteFor(100.0), 5.0);
        QCOMPARE(tolerance.toString(), QStringLiteral("5.00 %"));

        const eda::Measurement inside = eda::recordMeasurement(testPoint, 104.0,
                                                              QStringLiteral("kohm"));
        QCOMPARE(eda::toString(inside.status), eda::toString(eda::MeasurementStatus::Pass));
        const eda::Measurement outside = eda::recordMeasurement(testPoint, 106.0,
                                                               QStringLiteral("kohm"));
        QCOMPARE(eda::toString(outside.status), eda::toString(eda::MeasurementStatus::Fail));
        // The stored tolerance is the absolute number, so it stays readable.
        QCOMPARE(outside.tolerance, 5.0);
    }

    void withoutAnExpectedValueNothingCanPassOrFail()
    {
        eda::TestPoint testPoint;
        testPoint.uid = QStringLiteral("tp-unknown");
        testPoint.identifier = QStringLiteral("TP1");
        testPoint.unit = QStringLiteral("V");
        QVERIFY(!testPoint.hasExpectedValue);

        QString explanation;
        const eda::Measurement measurement = eda::recordMeasurement(testPoint, 3.3,
                                                                    QStringLiteral("V"));
        QCOMPARE(eda::toString(measurement.status), eda::toString(eda::MeasurementStatus::Unknown));
        QVERIFY(!measurement.hasExpectedValue);
        eda::evaluateMeasurement(measurement, &explanation);
        QVERIFY2(explanation.contains(QStringLiteral("no expected value")), qPrintable(explanation));

        // A stored measurement that was never compared reports "unknown", never
        // "not tested" and never "pass".
        const eda::Measurement stored = eda::Measurement::fromJson(measurement.toJson());
        QCOMPARE(eda::toString(stored.status), eda::toString(eda::MeasurementStatus::Unknown));
    }

    void differentUnitsAreNotCompared()
    {
        const eda::TestPoint testPoint = point(5.0, QStringLiteral("V"), 0.25);
        QString explanation;
        const eda::Measurement measurement = eda::recordMeasurement(testPoint, 4800.0,
                                                                    QStringLiteral("mV"));
        // mV and V mean the same thing, so the comparison is made.
        QCOMPARE(eda::toString(measurement.status), eda::toString(eda::MeasurementStatus::Pass));

        const eda::Measurement nonsense = eda::recordMeasurement(testPoint, 5.0,
                                                                 QStringLiteral("ohm"));
        QCOMPARE(eda::toString(nonsense.status), eda::toString(eda::MeasurementStatus::Unknown));
        eda::evaluateMeasurement(nonsense, &explanation);
        QVERIFY2(explanation.contains(QStringLiteral("not compared")), qPrintable(explanation));
    }

    void unitNamesAreComparedByMeaning()
    {
        QVERIFY(eda::unitsAreCompatible(QStringLiteral("V"), QStringLiteral("volt")));
        QVERIFY(eda::unitsAreCompatible(QStringLiteral("mV"), QStringLiteral("V")));
        QVERIFY(eda::unitsAreCompatible(QStringLiteral("kohm"), QStringLiteral("Ohm")));
        QVERIFY(eda::unitsAreCompatible(QStringLiteral("degC"), QStringLiteral("celsius")));
        QVERIFY(eda::unitsAreCompatible(QString(), QStringLiteral("V"))); // nothing said: no conflict
        QVERIFY(!eda::unitsAreCompatible(QStringLiteral("V"), QStringLiteral("A")));
        QVERIFY(!eda::unitsAreCompatible(QStringLiteral("Hz"), QStringLiteral("V")));
    }

    void valuesAreFormattedWithAPrefix()
    {
        QCOMPARE(eda::formatValue(4.7, QStringLiteral("V")), QStringLiteral("4.7 V"));
        QCOMPARE(eda::formatValue(4700.0, QStringLiteral("ohm")), QStringLiteral("4.7 kohm"));
        QCOMPARE(eda::formatValue(0.0000001, QStringLiteral("F")), QStringLiteral("100 nF"));
        QCOMPARE(eda::formatValue(0.0, QStringLiteral("V")), QStringLiteral("0 V"));
        QVERIFY(eda::formatValue(std::nan(""), QStringLiteral("V")).isEmpty() == false);
    }

    void coverageCountsWhatWasDoneAndWhatWasNot()
    {
        const eda::TestPoint first = point(5.0, QStringLiteral("V"), 0.25);
        eda::TestPoint second = point(3.3, QStringLiteral("V"), 0.1);
        second.uid = QStringLiteral("tp-3v3");
        eda::TestPoint third;
        third.uid = QStringLiteral("tp-nothing");
        third.identifier = QStringLiteral("TP3");
        third.unit = QStringLiteral("V");

        QVector<eda::Measurement> measurements;
        measurements.append(eda::recordMeasurement(first, 5.01, QStringLiteral("V")));
        measurements.append(eda::recordMeasurement(second, 2.9, QStringLiteral("V")));

        const eda::TestCoverage coverage =
            eda::coverageOf({ first, second, third }, measurements);
        QCOMPARE(coverage.testPoints, 3);
        QCOMPARE(coverage.measured, 2);
        QCOMPARE(coverage.passing, 1);
        QCOMPARE(coverage.failing, 1);
        QCOMPARE(coverage.untested, 1);
        QCOMPARE(coverage.unknown, 0);
        QVERIFY2(coverage.summary().contains(QStringLiteral("1 pass")), qPrintable(coverage.summary()));
        QVERIFY2(coverage.summary().contains(QStringLiteral("1 fail")), qPrintable(coverage.summary()));
        QVERIFY2(coverage.summary().contains(QStringLiteral("1 not tested")),
                 qPrintable(coverage.summary()));
    }

    void theNewestMeasurementSaysWhatTheBoardDoesNow()
    {
        const eda::TestPoint testPoint = point(5.0, QStringLiteral("V"), 0.25);
        QVector<eda::Measurement> measurements;
        eda::Measurement good = eda::recordMeasurement(
            testPoint, 5.0, QStringLiteral("V"),
            QDateTime::fromString(QStringLiteral("2024-01-01T10:00:00Z"), Qt::ISODate));
        eda::Measurement bad = eda::recordMeasurement(
            testPoint, 4.0, QStringLiteral("V"),
            QDateTime::fromString(QStringLiteral("2024-06-01T10:00:00Z"), Qt::ISODate));
        measurements.append(good);
        measurements.append(bad);

        const eda::Measurement latest = eda::latestMeasurement(testPoint.uid, measurements);
        QCOMPARE(latest.uid, bad.uid);
        QCOMPARE(eda::toString(latest.status), eda::toString(eda::MeasurementStatus::Fail));

        const eda::TestCoverage coverage = eda::coverageOf({ testPoint }, measurements);
        QCOMPARE(coverage.failing, 1);
        QCOMPARE(coverage.passing, 0);

        // Nothing measured at all is "not tested", not "failed".
        const eda::Measurement none = eda::latestMeasurement(QStringLiteral("nope"), measurements);
        QCOMPARE(eda::toString(none.status), eda::toString(eda::MeasurementStatus::NotTested));
    }

    void testPointIdentifiersAreNeverReused()
    {
        eda::Board board;
        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("C1");
        eda::Pad pad;
        pad.id = QStringLiteral("2");
        pad.pin = QStringLiteral("2");
        component.pads.append(pad);
        board.components.append(component);

        QVector<eda::TestPoint> existing;
        const QString first = eda::suggestTestPointIdentifier(board, QStringLiteral("c-1"),
                                                              QStringLiteral("2"), existing);
        QCOMPARE(first, QStringLiteral("C1.2"));
        existing.append(eda::TestPoint{});
        existing.last().identifier = first;
        const QString second = eda::suggestTestPointIdentifier(board, QStringLiteral("c-1"),
                                                               QStringLiteral("2"), existing);
        QVERIFY2(second != first, qPrintable(second));
        // Without a component the identifier is a plain test point name.
        const QString bare = eda::suggestTestPointIdentifier(board, QString(), QString(),
                                                            existing);
        QCOMPARE(bare, QStringLiteral("TP"));
    }

    void testPointsAndMeasurementsSurviveTheProjectDocument()
    {
        const QString path = QDir::cleanPath(
            QStringLiteral("%1/measurements.pwb").arg(eda::TestEnvironment::root()));
        QString error;
        eda::Project project;
        QVERIFY2(project.create(QStringLiteral("measurements"), eda::TestEnvironment::root(),
                                &error), qPrintable(error));

        eda::TestPoint testPoint = point(12.0, QStringLiteral("V"), 0.6);
        testPoint.identifier = QStringLiteral("J1.1");
        testPoint.componentUid = QStringLiteral("c-1");
        testPoint.pinOrPad = QStringLiteral("1");
        testPoint.netName = QStringLiteral("VIN");
        testPoint.toleranceIsPercent = false;
        testPoint.position = { 10.0, 20.0 };
        testPoint.hasPosition = true;
        testPoint.testConditions = QStringLiteral("board powered from 15 V");
        testPoint.provenance.location = QStringLiteral("datasheet page 3");
        project.data().testPoints.append(testPoint);

        eda::Measurement measurement = eda::recordMeasurement(testPoint, 11.87,
                                                             QStringLiteral("V"));
        measurement.instrument = QStringLiteral("Fluke 87V");
        project.data().measurements.append(measurement);

        QVERIFY2(project.save(&error), qPrintable(error));

        eda::Project reopened;
        QVERIFY2(reopened.open(path, &error), qPrintable(error));
        QCOMPARE(reopened.data().testPoints.size(), 1);
        QCOMPARE(reopened.data().measurements.size(), 1);
        const eda::TestPoint stored = reopened.data().testPoints.first();
        QCOMPARE(stored.identifier, QStringLiteral("J1.1"));
        QCOMPARE(stored.expectedValue, 12.0);
        QCOMPARE(stored.tolerance, 0.6);
        QCOMPARE(stored.unit, QStringLiteral("V"));
        QCOMPARE(stored.position.x, 10.0);
        QVERIFY(stored.hasPosition);
        QCOMPARE(stored.provenance.location, QStringLiteral("datasheet page 3"));
        QCOMPARE(eda::toString(stored.provenance.status),
                 eda::toString(eda::VerificationStatus::OfficialSourceVerified));

        const eda::Measurement storedMeasurement = reopened.data().measurements.first();
        QCOMPARE(storedMeasurement.value, 11.87);
        QCOMPARE(storedMeasurement.instrument, QStringLiteral("Fluke 87V"));
        QCOMPARE(eda::toString(storedMeasurement.status),
                 eda::toString(eda::MeasurementStatus::Pass));
        QVERIFY2(eda::measurementExplanation(storedMeasurement).contains(QStringLiteral("inside")),
                 qPrintable(eda::measurementExplanation(storedMeasurement)));
        QVERIFY(project.close());
        QVERIFY(reopened.close());
    }
};

QTEST_APPLESS_MAIN(TestMeasurements)
#include "test_measurements.moc"
