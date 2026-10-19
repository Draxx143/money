// Integration tests for the diagnostic engine.
//
// The engine is allowed to say "this is likely" only where the project really
// contains something that says so. These tests are mostly about the opposite
// direction: an hypothesis with no evidence must stay an idea, a reading nobody
// can judge must not count, and a suggestion has to name the data it came from.
#include <QtTest>

#include "../../src/core/Diagnostics.h"
#include "../../src/core/Measurements.h"
#include "../../src/core/Project.h"
#include "../TestEnvironment.h"

#include <QDir>

namespace {

// A board with three nets: VCC carries C1 and U1, GND carries C1 and U1 as
// well, and LED carries R1 and D1.
eda::Board fixtureBoard()
{
    auto padOn = [](const QString &net) {
        eda::Pad pad;
        pad.id = QStringLiteral("1");
        pad.pin = QStringLiteral("1");
        pad.netName = net;
        return pad;
    };

    eda::Board board;
    board.uid = QStringLiteral("board-fixture");
    board.name = QStringLiteral("fixture board");

    const QStringList groups[] = {
        { QStringLiteral("C1"), QStringLiteral("U1") },   // VCC and GND
        { QStringLiteral("R1"), QStringLiteral("D1") },   // LED
    };
    const QStringList nets[] = { { QStringLiteral("VCC"), QStringLiteral("GND") },
                                 { QStringLiteral("LED") } };

    for (int group = 0; group < 2; ++group) {
        for (const QString &reference : groups[group]) {
            eda::Component component;
            component.uid = QStringLiteral("comp-%1").arg(reference);
            component.reference = reference;
            component.nets = nets[group];
            component.pads.append(padOn(nets[group].first()));
            board.components.append(component);
        }
    }
    for (int group = 0; group < 2; ++group) {
        for (const QString &netName : nets[group]) {
            eda::Net net;
            net.name = netName;
            net.confirmed = true;
            for (const QString &reference : groups[group])
                net.padIds.append(QStringLiteral("comp-%1:1").arg(reference));
            board.nets.append(net);
        }
    }
    return board;
}

} // namespace

class TestDiagnostics : public QObject
{
    Q_OBJECT

private:
    eda::Project *m_project = nullptr;

    eda::TestPoint addPoint(const QString &identifier, const QString &netName,
                            const QString &componentUid, double expected = -1.0)
    {
        eda::TestPoint point;
        point.uid = QStringLiteral("tp-%1").arg(identifier);
        point.identifier = identifier;
        point.netName = netName;
        point.componentUid = componentUid;
        point.category = QStringLiteral("voltage");
        point.unit = QStringLiteral("V");
        point.provenance.status = eda::VerificationStatus::OfficialSourceVerified;
        point.provenance.valueSource = eda::ValueSource::ExtractedFromSource;
        if (expected >= 0.0) {
            point.expectedValue = expected;
            point.hasExpectedValue = true;
            point.tolerance = 0.25;
            point.hasTolerance = true;
        }
        m_project->data().testPoints.append(point);
        return point;
    }

    void record(const eda::TestPoint &point, double value, const QString &unit)
    {
        m_project->data().measurements.append(
            eda::recordMeasurement(point, value, unit));
    }

    QString addHypothesis(const QString &description,
                          const QStringList &components = QStringList())
    {
        eda::Hypothesis hypothesis;
        hypothesis.uid = QStringLiteral("h-%1").arg(m_project->data().hypotheses.size() + 1);
        hypothesis.description = description;
        hypothesis.componentUids = components;
        m_project->data().hypotheses.append(hypothesis);
        return hypothesis.uid;
    }

    // One piece of evidence, in the terms the engine weighs it.
    eda::Evidence evidence(const QString &hypothesisUid, eda::EvidenceKind kind,
                           eda::EvidenceDirection direction = eda::EvidenceDirection::Supports,
                           eda::VerificationStatus status = eda::VerificationStatus::UserVerified,
                           const QString &statement = QStringLiteral("it was checked on the bench"))
    {
        eda::Evidence item;
        item.hypothesisUid = hypothesisUid;
        item.kind = kind;
        item.direction = direction;
        item.statement = statement;
        item.reason = QStringLiteral("recorded by the test");
        item.provenance.status = status;
        return item;
    }

    // The Qt test macros may only be used in void functions; the id of what was
    // stored is read back from the project instead.
    void store(const eda::Evidence &item)
    {
        QString error;
        const QString uid = eda::addEvidence(m_project, item, &error);
        QVERIFY2(!uid.isEmpty(), qPrintable(error));
        Q_UNUSED(uid)
    }

private slots:
    void initTestCase() { eda::TestEnvironment::init(QStringLiteral("diagnostics")); }
    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void init()
    {
        m_project = new eda::Project(this);
        QString error;
        if (m_project->isOpen())
            m_project->close();
        QVERIFY2(m_project->create(QStringLiteral("diagnostics test"),
                                   eda::TestEnvironment::root(), &error),
                 qPrintable(error));
    }

    void cleanup()
    {
        delete m_project;
        m_project = nullptr;
    }

    void anHypothesisWithoutEvidenceIsOnlyAnIdea()
    {
        addHypothesis(QStringLiteral("the regulator is dead"));
        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);

        QCOMPARE(score.supporting, 0);
        QCOMPARE(score.contradicting, 0);
        QCOMPARE(score.score, 0.0);
        QCOMPARE(eda::toString(score.suggested),
                 eda::toString(eda::HypothesisStatus::Possible));
        QVERIFY2(score.summary.contains(QStringLiteral("no evidence recorded yet")),
                 qPrintable(score.summary));
        QVERIFY2(!score.summary.contains(QStringLiteral("plausible")),
                 qPrintable(score.summary));
    }

    void evidenceIsWeighedByWhatItIs()
    {
        // A repair says more than a reading, a reading says more than a
        // datasheet, and prior knowledge is only a hint.
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::Repair)), 3.0);
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::Measurement,
                                        eda::EvidenceDirection::Supports,
                                        eda::VerificationStatus::ExperimentallyVerified)),
                 2.0);
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::SourceDocument)),
                 1.0);
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::Observation)),
                 0.5);
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::PriorKnowledge)),
                 0.25);
        QCOMPARE(eda::weightOf(evidence(QStringLiteral("h"), eda::EvidenceKind::Unknown)), 0.1);
    }

    void anUnconfirmedClaimCountsLessThanACheckedOne()
    {
        const eda::Evidence checked =
            evidence(QStringLiteral("h"), eda::EvidenceKind::Observation,
                     eda::EvidenceDirection::Supports, eda::VerificationStatus::UserVerified);
        eda::Evidence claimed = checked;
        claimed.provenance.status = eda::VerificationStatus::Unverified;
        eda::Evidence inferred = checked;
        inferred.provenance.status = eda::VerificationStatus::Inferred;

        QVERIFY(eda::weightOf(claimed) < eda::weightOf(checked));
        QVERIFY(eda::weightOf(inferred) < eda::weightOf(checked));
        QVERIFY2(eda::weightExplanation(claimed).contains(QStringLiteral("no source")),
                 qPrintable(eda::weightExplanation(claimed)));
    }

    void aSuggestedClaimCountsHalf()
    {
        eda::Evidence suggested =
            evidence(QStringLiteral("h"), eda::EvidenceKind::Observation);
        suggested.provenance.valueSource = eda::ValueSource::AiSuggested;
        QCOMPARE(eda::weightOf(suggested), 0.25);
        QVERIFY2(eda::weightExplanation(suggested).contains(QStringLiteral("suggested")),
                 qPrintable(eda::weightExplanation(suggested)));
    }

    void aRepairConfirmsTheHypothesis()
    {
        const QString uid = addHypothesis(QStringLiteral("C1 is open circuit"));
        store(evidence(uid, eda::EvidenceKind::Measurement));
        store(evidence(uid, eda::EvidenceKind::Repair,
                       eda::EvidenceDirection::Supports, eda::VerificationStatus::UserVerified,
                       QStringLiteral("replacing C1 brought the rail back")));

        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);
        QCOMPARE(eda::toString(score.suggested),
                 eda::toString(eda::HypothesisStatus::ConfirmedByRepair));
        QVERIFY2(score.summary.contains(QStringLiteral("confirmed by repair")),
                 qPrintable(score.summary));
        // The newest evidence is explained first.
        QVERIFY2(score.reasons.first().contains(QStringLiteral("replacing C1")),
                 qPrintable(score.reasons.first()));
    }

    void aMeasurementOnTheBenchConfirmsTheHypothesis()
    {
        const QString uid = addHypothesis(QStringLiteral("the rail is shorted"));
        eda::Evidence reading =
            evidence(uid, eda::EvidenceKind::Measurement, eda::EvidenceDirection::Supports,
                     eda::VerificationStatus::ExperimentallyVerified,
                     QStringLiteral("the rail measures 0.4 ohm to ground"));
        reading.provenance.valueSource = eda::ValueSource::Measured;
        store(reading);

        // One reading on its own is not a confirmation, however clear it looks.
        QCOMPARE(eda::toString(eda::scoreHypothesis(m_project->data().hypotheses.first(),
                                                    m_project->data().evidence).suggested),
                 eda::toString(eda::HypothesisStatus::Plausible));

        store(evidence(uid, eda::EvidenceKind::Observation, eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::UserVerified,
                       QStringLiteral("U1 gets hot within two seconds")));
        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);
        QCOMPARE(eda::toString(score.suggested),
                 eda::toString(eda::HypothesisStatus::ConfirmedByMeasurement));
    }

    void theScoreGrowsWithEverySupportingPiece()
    {
        const QString uid = addHypothesis(QStringLiteral("U1 is damaged"));
        store(evidence(uid, eda::EvidenceKind::Observation));
        const double one =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence)
                .score;
        store(evidence(uid, eda::EvidenceKind::SourceDocument,
                       eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::OfficialSourceVerified));
        const double two =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence)
                .score;
        store(evidence(uid, eda::EvidenceKind::SourceDocument,
                       eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::Corroborated,
                       QStringLiteral("the errata sheet names this failure mode")));
        const eda::HypothesisScore three =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);

        QVERIFY2(one > 0.0 && one < two && two < three.score,
                 qPrintable(QStringLiteral("%1 < %2 < %3")
                                .arg(one).arg(two).arg(three.score)));
        QCOMPARE(three.supporting, 3);
        QCOMPARE(eda::toString(three.suggested),
                 eda::toString(eda::HypothesisStatus::StronglySupported));
    }

    void whatSpeaksAgainstCanRuleItOut()
    {
        const QString uid = addHypothesis(QStringLiteral("the fuse is blown"));
        store(evidence(uid, eda::EvidenceKind::Observation));
        store(evidence(uid, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Contradicts,
                       eda::VerificationStatus::ExperimentallyVerified,
                       QStringLiteral("the fuse measures 0.2 ohm end to end")));

        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);
        QCOMPARE(score.contradicting, 1);
        QVERIFY2(score.contradiction > score.support,
                 qPrintable(QStringLiteral("%1 against %2")
                                .arg(score.contradiction).arg(score.support)));
        QCOMPARE(eda::toString(score.suggested), eda::toString(eda::HypothesisStatus::RuledOut));
        QVERIFY2(score.summary.contains(QStringLiteral("ruled out")), qPrintable(score.summary));
    }

    void oneStrongCounterArgumentIsEnough()
    {
        const QString uid = addHypothesis(QStringLiteral("the crystal is dead"));
        store(evidence(uid, eda::EvidenceKind::Observation));
        store(evidence(uid, eda::EvidenceKind::PriorKnowledge));
        const double before =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence)
                .score;
        store(evidence(uid, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Contradicts,
                       eda::VerificationStatus::ExperimentallyVerified,
                       QStringLiteral("the crystal oscillates at 16 MHz")));
        const double after =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence)
                .score;
        QVERIFY2(after < before, qPrintable(QStringLiteral("%1 -> %2").arg(before).arg(after)));
    }

    void aReadingThatCouldNotBeJudgedWeighsNothing()
    {
        m_project->data().boards.append(fixtureBoard());
        const eda::TestPoint point = addPoint(QStringLiteral("GND"), QStringLiteral("GND"),
                                              QStringLiteral("comp-C1"));   // no expectation
        record(point, 0.4, QStringLiteral("V"));

        const QString uid = addHypothesis(QStringLiteral("the ground plane is broken"));
        const eda::Measurement stored = m_project->data().measurements.first();
        QCOMPARE(eda::toString(stored.status), eda::toString(eda::MeasurementStatus::Unknown));

        eda::Evidence item =
            eda::evidenceFromMeasurement(stored, point, eda::EvidenceDirection::Supports);
        item.hypothesisUid = uid;
        QVERIFY2(item.inconclusive, "a reading without an expectation is not evidence");
        QCOMPARE(eda::weightOf(item), 0.0);
        QVERIFY2(eda::weightExplanation(item).contains(QStringLiteral("could not be judged")),
                 qPrintable(eda::weightExplanation(item)));

        store(item);
        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);
        QCOMPARE(score.score, 0.0);
        QCOMPARE(eda::toString(score.suggested),
                 eda::toString(eda::HypothesisStatus::Possible));
        // It is still on record: the gap is worth reporting.
        QCOMPARE(score.items.size(), 1);
        QVERIFY2(score.reasons.first().contains(QStringLiteral("cannot be judged")),
                 qPrintable(score.reasons.first()));
    }

    void candidatesComeOnlyFromReadingsThatFailed()
    {
        m_project->data().boards.append(fixtureBoard());
        const eda::TestPoint vcc = addPoint(QStringLiteral("VCC"), QStringLiteral("VCC"),
                                            QStringLiteral("comp-C1"), 5.0);
        const eda::TestPoint led = addPoint(QStringLiteral("LED"), QStringLiteral("LED"),
                                            QStringLiteral("comp-R1"), 2.0);
        const eda::TestPoint gnd = addPoint(QStringLiteral("GND"), QStringLiteral("GND"),
                                            QStringLiteral("comp-C1"));
        record(vcc, 4.0, QStringLiteral("V"));    // outside 5 V +/-0.25 V
        record(led, 2.05, QStringLiteral("V"));   // inside: healthy
        record(gnd, 0.02, QStringLiteral("V"));   // no expectation: unknown

        const QVector<eda::HypothesisCandidate> candidates =
            eda::candidatesFromData(m_project->data());
        QVERIFY(!candidates.isEmpty());

        for (const eda::HypothesisCandidate &candidate : candidates) {
            QVERIFY2(candidate.reason.contains(QStringLiteral("VCC")),
                     qPrintable(candidate.reason));
            QVERIFY2(!candidate.reason.contains(QStringLiteral("LED")),
                     qPrintable(candidate.reason));
            QVERIFY2(!candidate.reason.contains(QStringLiteral("GND")),
                     qPrintable(candidate.reason));
        }
        bool hasNet = false;
        for (const eda::HypothesisCandidate &candidate : candidates) {
            if (candidate.description == QStringLiteral("the fault is on net VCC"))
                hasNet = true;
            QVERIFY2(!candidate.description.isEmpty(), "every candidate says something");
            QVERIFY2(candidate.prior > 0.0, "every candidate says how much it is worth");
        }
        QVERIFY2(hasNet, "the failing net is named");
    }

    void candidatesNameThePartsOnTheFailingNet()
    {
        m_project->data().boards.append(fixtureBoard());
        const eda::TestPoint vcc = addPoint(QStringLiteral("VCC"), QStringLiteral("VCC"),
                                            QStringLiteral("comp-C1"), 5.0);
        record(vcc, 0.0, QStringLiteral("V"));

        const QVector<eda::HypothesisCandidate> candidates =
            eda::candidatesFromData(m_project->data());
        QCOMPARE(candidates.size(), 3);   // the net and the two parts on it

        const eda::HypothesisCandidate &net = candidates.first();
        QCOMPARE(net.componentUids, QStringList({ QStringLiteral("comp-C1"),
                                                  QStringLiteral("comp-U1") }));
        QCOMPARE(candidates.at(1).componentUids,
                 QStringList({ QStringLiteral("comp-C1") }));
        QCOMPARE(candidates.at(2).componentUids,
                 QStringList({ QStringLiteral("comp-U1") }));
        QVERIFY2(candidates.at(1).description.contains(QStringLiteral("C1")),
                 qPrintable(candidates.at(1).description));
        QVERIFY2(candidates.at(1).reason.contains(QStringLiteral("one of 2 components")),
                 qPrintable(candidates.at(1).reason));
        // One part out of two is worth half of what the net is worth.
        QVERIFY(candidates.at(1).prior < net.prior);
    }

    void theNextTestIsOnTheNetThatAlreadyFailed()
    {
        m_project->data().boards.append(fixtureBoard());
        const eda::TestPoint vcc = addPoint(QStringLiteral("VCC"), QStringLiteral("VCC"),
                                            QStringLiteral("comp-C1"), 5.0);
        const eda::TestPoint second = addPoint(QStringLiteral("VCC2"), QStringLiteral("VCC"),
                                               QStringLiteral("comp-U1"), 5.0);
        const eda::TestPoint led = addPoint(QStringLiteral("LED"), QStringLiteral("LED"),
                                            QStringLiteral("comp-R1"), 2.0);
        record(vcc, 4.0, QStringLiteral("V"));
        Q_UNUSED(second)
        Q_UNUSED(led)

        const QVector<eda::SuggestedTest> suggestions =
            eda::suggestNextTests(m_project->data());
        QCOMPARE(suggestions.size(), 2);   // VCC2 and LED; VCC is judged already
        QCOMPARE(suggestions.first().testPointUid, QStringLiteral("tp-VCC2"));
        QVERIFY2(suggestions.first().reason.contains(QStringLiteral("net VCC")),
                 qPrintable(suggestions.first().reason));
        QVERIFY2(suggestions.first().information > suggestions.last().information,
                 qPrintable(QStringLiteral("%1 > %2")
                                .arg(suggestions.first().information)
                                .arg(suggestions.last().information)));
    }

    void aReadingWithoutAnExpectationIsAGapToClose()
    {
        m_project->data().boards.append(fixtureBoard());
        const eda::TestPoint gnd = addPoint(QStringLiteral("GND"), QStringLiteral("GND"),
                                            QStringLiteral("comp-C1"));
        record(gnd, 0.02, QStringLiteral("V"));

        const QVector<eda::SuggestedTest> suggestions =
            eda::suggestNextTests(m_project->data());
        QCOMPARE(suggestions.size(), 1);
        QVERIFY2(suggestions.first().reason.contains(QStringLiteral("proves nothing yet")),
                 qPrintable(suggestions.first().reason));
        QVERIFY2(suggestions.first().reason.contains(QStringLiteral("cannot pass or fail")),
                 qPrintable(suggestions.first().reason));
    }

    void anHypothesisPointsAtTheNextTest()
    {
        m_project->data().boards.append(fixtureBoard());
        addPoint(QStringLiteral("LED"), QStringLiteral("LED"), QStringLiteral("comp-R1"), 2.0);
        addPoint(QStringLiteral("VCC"), QStringLiteral("VCC"), QStringLiteral("comp-C1"), 5.0);

        const QString uid = addHypothesis(QStringLiteral("R1 is out of value"),
                                         QStringList{ QStringLiteral("comp-R1") });
        store(evidence(uid, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::ExperimentallyVerified,
                       QStringLiteral("R1 reads 4.7 kohm against 1 kohm")));

        const QVector<eda::HypothesisScore> scores =
            eda::scoreAll(m_project->data().hypotheses, m_project->data().evidence);
        const QVector<eda::SuggestedTest> suggestions =
            eda::suggestNextTests(m_project->data(), scores);

        QCOMPARE(suggestions.first().testPointUid, QStringLiteral("tp-LED"));
        QVERIFY2(suggestions.first().reason.contains(QStringLiteral("R1")),
                 qPrintable(suggestions.first().reason));
        QVERIFY2(suggestions.first().information > suggestions.last().information,
                 qPrintable(suggestions.first().reason));
    }

    void aSessionCollectsTheWork()
    {
        QString error;
        const QString sessionUid = eda::startSession(
            m_project, QStringLiteral("board dead on arrival"),
            QStringList{ QStringLiteral("no 5 V"), QStringLiteral("no LED") }, &error);
        QVERIFY2(!sessionUid.isEmpty(), qPrintable(error));
        QVERIFY(eda::activeSession(m_project) != nullptr);
        QCOMPARE(eda::activeSession(m_project)->uid, sessionUid);

        const QString uid = addHypothesis(QStringLiteral("the regulator is dead"));
        store(evidence(uid, eda::EvidenceKind::Observation));
        // Evidence recorded during a session belongs to it.
        QCOMPARE(m_project->data().evidence.first().sessionId, sessionUid);

        // A title is required: a session nobody can name is of no use later.
        QVERIFY2(eda::startSession(m_project, QStringLiteral("   "), {}, &error).isEmpty(),
                 "an empty title is refused");
        QVERIFY2(!error.isEmpty(), "and the reason is given");

        QVERIFY2(eda::closeSession(m_project, sessionUid,
                                   QStringLiteral("replaced U1, board works"), &error),
                 qPrintable(error));
        QVERIFY2(eda::activeSession(m_project) == nullptr, "a closed session is not active");
        QVERIFY2(!eda::closeSession(m_project, QStringLiteral("no-such-session"),
                                    QStringLiteral("whatever"), &error),
                 "an unknown session cannot be closed");
    }

    void theSummarySaysWhatIsKnownAndWhatIsNot()
    {
        const QString dead = addHypothesis(QStringLiteral("the regulator is dead"));
        const QString fuse = addHypothesis(QStringLiteral("the fuse is blown"));
        const QString crystal = addHypothesis(QStringLiteral("the crystal is dead"));
        store(evidence(dead, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::ExperimentallyVerified));
        store(evidence(dead, eda::EvidenceKind::Repair));
        store(evidence(fuse, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Contradicts,
                       eda::VerificationStatus::ExperimentallyVerified));
        Q_UNUSED(crystal)

        const QString sessionUid =
            eda::startSession(m_project, QStringLiteral("no power"), {}, nullptr);
        QVERIFY(!sessionUid.isEmpty());
        const QString summary = eda::sessionSummary(m_project->data(),
                                                    eda::activeSession(m_project));
        QVERIFY2(summary.contains(QStringLiteral("no power")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("3 hypotheses")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("1 ruled out")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("1 confirmed")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("2 with evidence")), qPrintable(summary));
    }

    void hypothesesAndEvidenceSurviveTheProject()
    {
        const QString uid = addHypothesis(QStringLiteral("C1 is open circuit"),
                                         QStringList{ QStringLiteral("comp-C1") });
        store(evidence(uid, eda::EvidenceKind::Measurement,
                       eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::ExperimentallyVerified,
                       QStringLiteral("C1 reads 12 nF instead of 100 nF")));
        store(evidence(uid, eda::EvidenceKind::Observation, eda::EvidenceDirection::Supports,
                       eda::VerificationStatus::UserVerified,
                       QStringLiteral("the rail is clean once C1 is lifted")));
        const QString sessionUid =
            eda::startSession(m_project, QStringLiteral("rail is unstable"), {}, nullptr);
        QVERIFY(!sessionUid.isEmpty());

        const QString path =
            eda::TestEnvironment::root() + QStringLiteral("/diagnostics.pwb");
        QString error;
        QVERIFY2(m_project->saveAs(path, &error), qPrintable(error));
        QVERIFY(m_project->close());
        QVERIFY2(m_project->open(path, &error), qPrintable(error));

        QCOMPARE(m_project->data().hypotheses.size(), 1);
        QCOMPARE(m_project->data().evidence.size(), 2);
        QCOMPARE(m_project->data().sessions.size(), 1);

        const eda::Evidence &stored = m_project->data().evidence.first();
        QCOMPARE(eda::toString(stored.kind), eda::toString(eda::EvidenceKind::Measurement));
        QCOMPARE(stored.statement, QStringLiteral("C1 reads 12 nF instead of 100 nF"));
        QVERIFY(stored.provenance.status == eda::VerificationStatus::ExperimentallyVerified);

        // The engine reaches the same verdict after a reload.
        const eda::HypothesisScore score =
            eda::scoreHypothesis(m_project->data().hypotheses.first(), m_project->data().evidence);
        QCOMPARE(score.supporting, 2);
        QCOMPARE(eda::toString(score.suggested),
                 eda::toString(eda::HypothesisStatus::ConfirmedByMeasurement));
        QCOMPARE(eda::activeSession(m_project)->title, QStringLiteral("rail is unstable"));
    }
};

QTEST_APPLESS_MAIN(TestDiagnostics)
#include "test_diagnostics.moc"
