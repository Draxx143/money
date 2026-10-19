// Test points, measurements and the verdict that compares them.
//
// This is the whole of the "is this value good or bad?" logic, and it is
// deliberately dull:
//
//   * a measurement without an expected value is never a pass and never a fail;
//     it is reported as not tested,
//   * a measurement in a different unit than the expectation is not compared at
//     all -- it is reported as unknown, with the two units named,
//   * a verdict is always accompanied by the sentence that explains it, so a
//     report can show the reasoning and not just a green tick.
//
// Nothing here rounds a number into a verdict: the tolerance is the user's, and
// if there is none, the answer is unknown and not "close enough".
#pragma once

#include "ProjectModel.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace eda {

// A tolerance, either absolute ("0.25 V") or a percentage of the expected
// value ("5 %").
struct Tolerance
{
    double value = 0.0;
    bool isPercent = false;
    bool hasValue = false;

    // The tolerance as an absolute number, for a given expected value.
    double absoluteFor(double expected) const
    {
        if (!hasValue)
            return 0.0;
        return isPercent ? std::abs(expected) * value / 100.0 : std::abs(value);
    }
    QString toString() const;
};

Tolerance toleranceOf(const TestPoint &point);

// Compares one measured value against an expectation. <explanation> is always
// filled in when it is given: the sentence a report shows next to the verdict.
MeasurementStatus evaluateMeasurement(double measured, double expected, bool hasExpected,
                                      const Tolerance &tolerance, const QString &measuredUnit,
                                      const QString &expectedUnit, QString *explanation = nullptr);

// The same, on a stored measurement (which carries its own snapshot of what was
// expected at the time it was taken).
MeasurementStatus evaluateMeasurement(const Measurement &measurement, QString *explanation = nullptr);

// Builds a measurement of a test point: copies the expectation that was valid
// at that moment, evaluates the verdict and writes the sentence that explains
// it into the notes when there is no note yet.
Measurement recordMeasurement(const TestPoint &point, double value, const QString &unit,
                              const QDateTime &timestamp = QDateTime::currentDateTimeUtc(),
                              const QString &instrument = QString(),
                              const QString &conditions = QString(),
                              const QString &notes = QString(),
                              const QString &sessionId = QString());

// One sentence: "4.87 V against 5.00 V +/-5 % -- inside limits".
QString measurementExplanation(const Measurement &measurement);

// --- units ---------------------------------------------------------------
// Units are compared by what they mean, so "V", "v" and "volt" are the same and
// "mV" is not. Unknown units are compared as written.
QString normaliseUnit(const QString &unit);
bool unitsAreCompatible(const QString &a, const QString &b);

// Values with a prefix, so a report reads "4.7 kOhm" and not "4700 ohm".
QString formatValue(double value, const QString &unit, int significantDigits = 4);

// --- how far along the board is ------------------------------------------
struct TestCoverage
{
    int testPoints = 0;
    int measured = 0;      // at least one measurement
    int passing = 0;
    int failing = 0;
    int untested = 0;      // no measurement at all
    int unknown = 0;       // measured, but the verdict could not be reached

    QString summary() const;
};

TestCoverage coverageOf(const QVector<TestPoint> &points, const QVector<Measurement> &measurements);

// The most recent measurement of a test point, or an empty one.
Measurement latestMeasurement(const QString &testPointUid,
                              const QVector<Measurement> &measurements);

// A default identifier for a new test point: "C1.2", "TP3", ...
QString suggestTestPointIdentifier(const Board &board, const QString &componentUid,
                                   const QString &pinOrPad,
                                   const QVector<TestPoint> &existing);

} // namespace eda
