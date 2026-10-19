#include "Measurements.h"

#include <QHash>
#include <QStringList>
#include <QUuid>

#include <cmath>
#include <algorithm>

namespace eda {
namespace {

struct Prefix
{
    double factor;
    const char *symbol;
};

// SI prefixes, from pico to giga.
const Prefix kPrefixes[] = {
    { 1e-12, "p" }, { 1e-9, "n" }, { 1e-6, "u" }, { 1e-3, "m" }, { 1.0, "" },
    { 1e3, "k" },   { 1e6, "M" },  { 1e9, "G" }
};

// What a written unit means. The key is the normalised form.
const QHash<QString, QString> &unitMeanings()
{
    static const QHash<QString, QString> meanings = {
        { QStringLiteral("v"), QStringLiteral("volt") },
        { QStringLiteral("volt"), QStringLiteral("volt") },
        { QStringLiteral("volts"), QStringLiteral("volt") },
        { QStringLiteral("mv"), QStringLiteral("volt") },
        { QStringLiteral("millivolt"), QStringLiteral("volt") },
        { QStringLiteral("a"), QStringLiteral("ampere") },
        { QStringLiteral("amp"), QStringLiteral("ampere") },
        { QStringLiteral("amps"), QStringLiteral("ampere") },
        { QStringLiteral("ampere"), QStringLiteral("ampere") },
        { QStringLiteral("ma"), QStringLiteral("ampere") },
        { QStringLiteral("milliamp"), QStringLiteral("ampere") },
        { QStringLiteral("ohm"), QStringLiteral("ohm") },
        { QStringLiteral("ohms"), QStringLiteral("ohm") },
        { QStringLiteral("r"), QStringLiteral("ohm") },
        { QStringLiteral("kohm"), QStringLiteral("ohm") },
        { QStringLiteral("kohms"), QStringLiteral("ohm") },
        { QStringLiteral("mohm"), QStringLiteral("ohm") },
        { QStringLiteral("f"), QStringLiteral("farad") },
        { QStringLiteral("farad"), QStringLiteral("farad") },
        { QStringLiteral("farads"), QStringLiteral("farad") },
        { QStringLiteral("nf"), QStringLiteral("farad") },
        { QStringLiteral("uf"), QStringLiteral("farad") },
        { QStringLiteral("pf"), QStringLiteral("farad") },
        { QStringLiteral("hz"), QStringLiteral("hertz") },
        { QStringLiteral("hertz"), QStringLiteral("hertz") },
        { QStringLiteral("khz"), QStringLiteral("hertz") },
        { QStringLiteral("mhz"), QStringLiteral("hertz") },
        { QStringLiteral("c"), QStringLiteral("degC") },
        { QStringLiteral("degc"), QStringLiteral("degC") },
        { QStringLiteral("celsius"), QStringLiteral("degC") },
        { QStringLiteral("degree"), QStringLiteral("degC") },
        { QStringLiteral("degrees"), QStringLiteral("degC") },
        { QStringLiteral("w"), QStringLiteral("watt") },
        { QStringLiteral("watt"), QStringLiteral("watt") },
        { QStringLiteral("watts"), QStringLiteral("watt") }
    };
    return meanings;
}

// The factor a unit carries in its prefix. Read from the unit as it was
// written, because case matters: "MHz" is a million hertz, "mHz" is a
// thousandth of one.
double prefixFactorOf(const QString &unit)
{
    if (unit.isEmpty())
        return 1.0;
    switch (unit.at(0).toLatin1()) {
    case 'p': return 1e-12;
    case 'n': return 1e-9;
    case 'u': return 1e-6;
    case 'm': return 1e-3;
    case 'k': return 1e3;
    case 'M': return 1e6;
    case 'G': return 1e9;
    default: return 1.0;
    }
}

} // namespace

QString Tolerance::toString() const
{
    if (!hasValue)
        return QString();
    return isPercent ? QStringLiteral("%1 %").arg(value, 0, 'f', 2)
                     : QStringLiteral("%1").arg(value, 0, 'f', 4);
}

Tolerance toleranceOf(const TestPoint &point)
{
    Tolerance tolerance;
    tolerance.value = point.tolerance;
    tolerance.isPercent = point.toleranceIsPercent;
    tolerance.hasValue = point.hasTolerance;
    return tolerance;
}

QString normaliseUnit(const QString &unit)
{
    QString text = unit.trimmed().toLower();
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0x03A9));   // the ohm sign, if somebody typed it
    return text;
}

bool unitsAreCompatible(const QString &a, const QString &b)
{
    const QString left = normaliseUnit(a);
    const QString right = normaliseUnit(b);
    if (left.isEmpty() || right.isEmpty())
        return true; // nothing was said about the unit: do not invent a conflict
    if (left == right)
        return true;
    const QString leftMeaning = unitMeanings().value(left, left);
    const QString rightMeaning = unitMeanings().value(right, right);
    return leftMeaning == rightMeaning;
}

QString formatValue(double value, const QString &unit, int significantDigits)
{
    if (!std::isfinite(value))
        return QStringLiteral("-");

    const double absolute = std::abs(value);
    if (absolute == 0.0)
        return QStringLiteral("0 %1").arg(unit);

    // Choose the prefix that keeps the number between 1 and 1000.
    double scaled = value;
    QString symbol;
    for (const Prefix &prefix : kPrefixes) {
        const double candidate = value / prefix.factor;
        if (std::abs(candidate) >= 1.0) {
            scaled = candidate;
            symbol = QString::fromLatin1(prefix.symbol);
        }
    }
    if (symbol.isEmpty() && absolute < 1.0) {
        scaled = value * 1e3;
        symbol = QStringLiteral("m");
    }

    QString number = QString::number(scaled, 'g', significantDigits);
    return QStringLiteral("%1 %2%3").arg(number, symbol, unit);
}

MeasurementStatus evaluateMeasurement(double measured, double expected, bool hasExpected,
                                      const Tolerance &tolerance, const QString &measuredUnit,
                                      const QString &expectedUnit, QString *explanation)
{
    auto explain = [explanation](const QString &text) {
        if (explanation)
            *explanation = text;
    };

    if (!hasExpected) {
        explain(QStringLiteral("no expected value was recorded, so this cannot pass or fail"));
        return MeasurementStatus::Unknown;
    }
    if (!unitsAreCompatible(measuredUnit, expectedUnit)) {
        explain(QStringLiteral("the measurement is in %1 but the expectation is in %2, so they "
                               "were not compared")
                    .arg(measuredUnit.isEmpty() ? QStringLiteral("(no unit)") : measuredUnit,
                         expectedUnit.isEmpty() ? QStringLiteral("(no unit)") : expectedUnit));
        return MeasurementStatus::Unknown;
    }

    // Both values are brought into the unit of the expectation first: 4800 mV
    // against 5 V is a comparison, not a mismatch.
    const double scale = prefixFactorOf(measuredUnit.trimmed())
                         / prefixFactorOf(expectedUnit.trimmed());
    const double limit = tolerance.absoluteFor(expected);
    const double difference = measured * scale - expected;
    const bool withinTolerance = std::abs(difference) <= limit + 1e-12;

    QString toleranceText;
    if (tolerance.hasValue) {
        toleranceText = tolerance.isPercent
                            ? QStringLiteral("+/-%1 % (%2)")
                                  .arg(tolerance.value, 0, 'f', 2)
                                  .arg(formatValue(limit, expectedUnit))
                            : QStringLiteral("+/-%1").arg(formatValue(limit, expectedUnit));
    } else {
        toleranceText = QStringLiteral("no tolerance");
    }

    explain(QStringLiteral("%1 against %2 %3 -- %4")
                .arg(formatValue(measured, measuredUnit), formatValue(expected, expectedUnit),
                     toleranceText,
                     withinTolerance ? QStringLiteral("inside the limits")
                                     : QStringLiteral("outside the limits")));
    return withinTolerance ? MeasurementStatus::Pass : MeasurementStatus::Fail;
}

MeasurementStatus evaluateMeasurement(const Measurement &measurement, QString *explanation)
{
    Tolerance tolerance;
    tolerance.value = measurement.tolerance;
    tolerance.isPercent = false;
    tolerance.hasValue = measurement.hasTolerance;
    // A stored measurement keeps the unit the expectation was written in. Old
    // records without it were written when both sides used the same unit.
    const QString expectedUnit = measurement.expectedUnit.isEmpty() ? measurement.unit
                                                                   : measurement.expectedUnit;
    return evaluateMeasurement(measurement.value, measurement.expectedValue,
                               measurement.hasExpectedValue, tolerance, measurement.unit,
                               expectedUnit, explanation);
}

Measurement recordMeasurement(const TestPoint &point, double value, const QString &unit,
                              const QDateTime &timestamp, const QString &instrument,
                              const QString &conditions, const QString &notes,
                              const QString &sessionId)
{
    Measurement measurement;
    measurement.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    measurement.testPointUid = point.uid;
    measurement.timestamp = timestamp.isValid() ? timestamp : QDateTime::currentDateTimeUtc();
    measurement.value = value;
    measurement.unit = unit.trimmed();
    measurement.expectedUnit = point.unit.trimmed();
    measurement.expectedValue = point.expectedValue;
    measurement.hasExpectedValue = point.hasExpectedValue;
    // The tolerance of a test point can be a percentage; a measurement stores
    // the absolute number, so that the verdict stays readable later.
    measurement.tolerance = toleranceOf(point).absoluteFor(point.expectedValue);
    measurement.hasTolerance = point.hasTolerance;
    measurement.instrument = instrument.trimmed();
    measurement.conditions = conditions.trimmed();
    measurement.source = ValueSource::Measured;
    measurement.sessionId = sessionId;

    QString explanation;
    measurement.status = evaluateMeasurement(value, point.expectedValue, point.hasExpectedValue,
                                             toleranceOf(point), measurement.unit, point.unit,
                                             &explanation);
    measurement.notes = notes.trimmed().isEmpty() ? explanation : notes.trimmed();
    return measurement;
}

QString measurementExplanation(const Measurement &measurement)
{
    QString explanation;
    evaluateMeasurement(measurement, &explanation);
    return explanation;
}

QString TestCoverage::summary() const
{
    return QStringLiteral("%1 test point(s): %2 measured, %3 pass, %4 fail, %5 not tested, "
                          "%6 without a verdict")
        .arg(testPoints)
        .arg(measured)
        .arg(passing)
        .arg(failing)
        .arg(untested)
        .arg(unknown);
}

TestCoverage coverageOf(const QVector<TestPoint> &points, const QVector<Measurement> &measurements)
{
    TestCoverage coverage;
    coverage.testPoints = points.size();
    for (const TestPoint &point : points) {
        QVector<Measurement> mine;
        for (const Measurement &measurement : measurements) {
            if (measurement.testPointUid == point.uid)
                mine.append(measurement);
        }
        if (mine.isEmpty()) {
            ++coverage.untested;
            continue;
        }
        ++coverage.measured;
        // The newest measurement says what the board does now.
        const Measurement latest = latestMeasurement(point.uid, measurements);
        switch (latest.status) {
        case MeasurementStatus::Pass:
            ++coverage.passing;
            break;
        case MeasurementStatus::Fail:
            ++coverage.failing;
            break;
        case MeasurementStatus::NotTested:
            ++coverage.untested;
            break;
        case MeasurementStatus::Unknown:
            ++coverage.unknown;
            break;
        }
    }
    return coverage;
}

Measurement latestMeasurement(const QString &testPointUid,
                              const QVector<Measurement> &measurements)
{
    Measurement latest;
    for (const Measurement &measurement : measurements) {
        if (measurement.testPointUid != testPointUid)
            continue;
        // ">=", not ">": two readings taken in the same millisecond are told
        // apart by the order they were written down in, and the one recorded
        // later is the newer one.
        if (latest.uid.isEmpty() || measurement.timestamp >= latest.timestamp)
            latest = measurement;
    }
    if (latest.uid.isEmpty())
        latest.status = MeasurementStatus::NotTested;
    return latest;
}

QString suggestTestPointIdentifier(const Board &board, const QString &componentUid,
                                   const QString &pinOrPad,
                                   const QVector<TestPoint> &existing)
{
    QString base;
    if (const Component *component = board.componentByUid(componentUid)) {
        base = component->reference;
        if (!pinOrPad.isEmpty())
            base += QStringLiteral(".") + pinOrPad;
    }
    if (base.isEmpty())
        base = QStringLiteral("TP");

    // Never reuse an identifier: two test points with the same name is exactly
    // the kind of confusion this is meant to prevent.
    int suffix = 0;
    for (;;) {
        const QString candidate = suffix == 0 ? base : QStringLiteral("%1-%2").arg(base).arg(suffix);
        bool taken = false;
        for (const TestPoint &point : existing) {
            if (point.identifier == candidate) {
                taken = true;
                break;
            }
        }
        if (!taken)
            return candidate;
        ++suffix;
    }
}

} // namespace eda
