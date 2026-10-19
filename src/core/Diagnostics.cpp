#include "Diagnostics.h"

#include "Measurements.h"
#include "Project.h"

#include <QDateTime>
#include <QUuid>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace eda {
namespace {

double clamp01(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

const TestPoint *findTestPoint(const ProjectData &data, const QString &uid)
{
    if (uid.isEmpty())
        return nullptr;
    for (const TestPoint &point : data.testPoints) {
        if (point.uid == uid)
            return &point;
    }
    return nullptr;
}

// Which components sit on a net. The net carries "<componentUid>:<padId>"
// entries; a component that names the net but has no pad on it is counted too,
// because the board data is not always complete enough to tell the difference.
QStringList componentsOnNet(const Board *board, const QString &netName)
{
    QStringList uids;
    if (board == nullptr || netName.isEmpty())
        return uids;

    QSet<QString> seen;
    for (const Net &net : board->nets) {
        if (net.name != netName)
            continue;
        for (const QString &padId : net.padIds) {
            const QString componentUid = padId.section(QLatin1Char(':'), 0, 0);
            if (!componentUid.isEmpty())
                seen.insert(componentUid);
        }
        break;
    }
    for (const Component &component : board->components) {
        if (component.nets.contains(netName))
            seen.insert(component.uid);
    }
    for (const Component &component : board->components) {
        if (seen.contains(component.uid))
            uids.append(component.uid);
    }
    return uids;
}

QString referenceOf(const Board *board, const QString &uid)
{
    if (board == nullptr)
        return QString();
    if (const Component *component = board->componentByUid(uid))
        return component->reference;
    return QString();
}

// Newest first: the list is walked backwards and the sort is stable, so two
// entries written in the same second keep the order they were written in.
QVector<Evidence> newestFirst(const QVector<Evidence> &evidence)
{
    QVector<Evidence> out;
    out.reserve(evidence.size());
    for (auto it = evidence.crbegin(); it != evidence.crend(); ++it)
        out.append(*it);
    std::stable_sort(out.begin(), out.end(), [](const Evidence &a, const Evidence &b) {
        return a.recorded > b.recorded;
    });
    return out;
}

QString number(double value)
{
    return QString::number(value, 'f', 2);
}

} // namespace

// ---------------------------------------------------------------------------
// Weights
// ---------------------------------------------------------------------------
double weightOf(const Evidence &evidence)
{
    // Something that could not be judged is not an argument for or against
    // anything. It is kept -- the gap is worth reporting -- but it weighs zero.
    if (evidence.inconclusive)
        return 0.0;

    double weight = 0.1;
    switch (evidence.kind) {
    case EvidenceKind::Repair:
        weight = 3.0;   // the fault changed when a part was changed: the best
        break;          // evidence there is
    case EvidenceKind::Measurement:
        weight = 2.0;
        break;
    case EvidenceKind::SourceDocument:
        weight = 1.0;
        break;
    case EvidenceKind::Observation:
        weight = 0.5;
        break;
    case EvidenceKind::PriorKnowledge:
        weight = 0.25;  // "this board always does this" is a hint, not a proof
        break;
    case EvidenceKind::Unknown:
        weight = 0.1;
        break;
    }

    // How trustworthy it is scales the weight down, never up: nothing is worth
    // more than what it is.
    switch (evidence.provenance.status) {
    case VerificationStatus::ExperimentallyVerified:
    case VerificationStatus::UserVerified:
    case VerificationStatus::OfficialSourceVerified:
    case VerificationStatus::Corroborated:
        break;
    case VerificationStatus::ExtractedFromSource:
        weight *= 0.8;
        break;
    case VerificationStatus::NeedsReview:
        weight *= 0.7;
        break;
    case VerificationStatus::Inferred:
        weight *= 0.6;
        break;
    case VerificationStatus::Unverified:
        weight *= 0.5;
        break;
    case VerificationStatus::AiSuggested:
        weight *= 0.4;
        break;
    case VerificationStatus::ConflictingSources:
        weight *= 0.3;
        break;
    case VerificationStatus::Unknown:
        break;   // no provenance given: the kind alone decides
    }

    // Something a model proposed is a suggestion until somebody checked it.
    if (evidence.provenance.valueSource == ValueSource::AiSuggested)
        weight *= 0.5;

    return weight;
}

QString weightExplanation(const Evidence &evidence)
{
    if (evidence.inconclusive)
        return QStringLiteral("the reading could not be judged, so it counts for nothing");

    QString kind;
    switch (evidence.kind) {
    case EvidenceKind::Repair:
        kind = QStringLiteral("a repair: the behaviour changed when a part was changed");
        break;
    case EvidenceKind::Measurement:
        kind = QStringLiteral("a measurement");
        break;
    case EvidenceKind::SourceDocument:
        kind = QStringLiteral("a source document");
        break;
    case EvidenceKind::Observation:
        kind = QStringLiteral("an observation on the bench");
        break;
    case EvidenceKind::PriorKnowledge:
        kind = QStringLiteral("prior knowledge, which is a hint and not a proof");
        break;
    case EvidenceKind::Unknown:
        kind = QStringLiteral("an unidentified piece of evidence");
        break;
    }

    QString trust;
    switch (evidence.provenance.status) {
    case VerificationStatus::ExperimentallyVerified:
        trust = QStringLiteral("confirmed by measurement");
        break;
    case VerificationStatus::UserVerified:
        trust = QStringLiteral("checked by the user");
        break;
    case VerificationStatus::OfficialSourceVerified:
        trust = QStringLiteral("confirmed by an official source");
        break;
    case VerificationStatus::Corroborated:
        trust = QStringLiteral("confirmed by more than one source");
        break;
    case VerificationStatus::ExtractedFromSource:
        trust = QStringLiteral("copied from an imported file, not confirmed");
        break;
    case VerificationStatus::NeedsReview:
        trust = QStringLiteral("recorded but not looked at yet");
        break;
    case VerificationStatus::Inferred:
        trust = QStringLiteral("derived from other data, not directly confirmed");
        break;
    case VerificationStatus::Unverified:
        trust = QStringLiteral("no source that could confirm it");
        break;
    case VerificationStatus::AiSuggested:
        trust = QStringLiteral("proposed by a model, not confirmed");
        break;
    case VerificationStatus::ConflictingSources:
        trust = QStringLiteral("the sources disagree");
        break;
    case VerificationStatus::Unknown:
        break;
    }

    QStringList parts(kind);
    if (!trust.isEmpty())
        parts.append(trust);
    if (evidence.provenance.valueSource == ValueSource::AiSuggested)
        parts.append(QStringLiteral("halved again because it was suggested, not measured"));
    return parts.join(QStringLiteral("; "));
}

ScoredEvidence scoreEvidence(const Evidence &evidence)
{
    ScoredEvidence out;
    out.evidence = evidence;
    out.weight = weightOf(evidence);

    QString direction;
    switch (evidence.direction) {
    case EvidenceDirection::Supports:
        direction = QStringLiteral("counts %1 for");
        break;
    case EvidenceDirection::Contradicts:
        direction = QStringLiteral("counts %1 against");
        break;
    case EvidenceDirection::Neutral:
        direction = QStringLiteral("counts %1, neither for nor against");
        break;
    }

    const QString statement = evidence.statement.trimmed().isEmpty()
                                  ? QStringLiteral("(no statement recorded)")
                                  : evidence.statement.trimmed();
    out.explanation = QStringLiteral("%1 -- %2 (%3)")
                          .arg(statement, direction.arg(number(out.weight)),
                               weightExplanation(evidence));
    return out;
}

// ---------------------------------------------------------------------------
// Scoring
// ---------------------------------------------------------------------------
HypothesisScore scoreHypothesis(const Hypothesis &hypothesis, const QVector<Evidence> &evidence)
{
    HypothesisScore score;

    for (const Evidence &item : newestFirst(evidenceFor(evidence, hypothesis.uid))) {
        const ScoredEvidence scored = scoreEvidence(item);
        score.items.append(scored);
        score.reasons.append(scored.explanation);
        switch (item.direction) {
        case EvidenceDirection::Supports:
            score.support += scored.weight;
            ++score.supporting;
            break;
        case EvidenceDirection::Contradicts:
            score.contradiction += scored.weight;
            ++score.contradicting;
            break;
        case EvidenceDirection::Neutral:
            ++score.neutral;
            break;
        }
    }

    // How much evidence there is, and which way it points. With no evidence at
    // all the score is exactly zero: an idea is not a finding.
    const double total = score.support + score.contradiction;
    const double strength = 1.0 - std::exp(-total / 2.0);
    const double balance = total > 0.0 ? score.support / total : 0.5;
    score.score = clamp01(strength * balance);
    score.suggested = statusFromScore(score);
    score.summary = scoreSummary(score);
    return score;
}

QVector<HypothesisScore> scoreAll(const QVector<Hypothesis> &hypotheses,
                                  const QVector<Evidence> &evidence)
{
    QVector<HypothesisScore> out;
    out.reserve(hypotheses.size());
    for (const Hypothesis &hypothesis : hypotheses)
        out.append(scoreHypothesis(hypothesis, evidence));
    return out;
}

HypothesisStatus statusFromScore(const HypothesisScore &score)
{
    // The strongest single pieces of evidence decide first: one successful
    // repair outweighs a pile of circumstantial support.
    for (const ScoredEvidence &item : score.items) {
        if (item.evidence.direction != EvidenceDirection::Supports)
            continue;
        if (item.evidence.kind == EvidenceKind::Repair && item.weight >= 2.0)
            return HypothesisStatus::ConfirmedByRepair;
        // A single reading is not a confirmation, however clear it looks: it
        // takes a reading plus something else, or two of them, before the
        // engine calls a hypothesis confirmed.
        if (item.evidence.kind == EvidenceKind::Measurement && item.weight >= 1.5
            && score.score >= 0.70
            && (item.evidence.provenance.status == VerificationStatus::ExperimentallyVerified
                || item.evidence.provenance.valueSource == ValueSource::Measured)) {
            return HypothesisStatus::ConfirmedByMeasurement;
        }
    }

    if (score.contradicting > 0 && score.contradiction >= score.support)
        return HypothesisStatus::RuledOut;

    if (score.supporting == 0)
        return HypothesisStatus::Possible;   // nothing measured, nothing claimed

    if (score.score >= 0.70 && score.supporting >= 2)
        return HypothesisStatus::StronglySupported;
    if (score.score >= 0.40)
        return HypothesisStatus::Plausible;
    return HypothesisStatus::Possible;
}

QString statusLabel(HypothesisStatus status)
{
    switch (status) {
    case HypothesisStatus::Possible: return QStringLiteral("Possible");
    case HypothesisStatus::Plausible: return QStringLiteral("Plausible");
    case HypothesisStatus::StronglySupported: return QStringLiteral("Strongly supported");
    case HypothesisStatus::ConfirmedByMeasurement: return QStringLiteral("Confirmed by measurement");
    case HypothesisStatus::ConfirmedByRepair: return QStringLiteral("Confirmed by repair");
    case HypothesisStatus::RuledOut: return QStringLiteral("Ruled out");
    case HypothesisStatus::Unresolved: break;
    }
    return QStringLiteral("Unresolved");
}

QString scoreSummary(const HypothesisScore &score)
{
    if (score.supporting == 0 && score.contradicting == 0)
        return QStringLiteral("no evidence recorded yet: this is an idea, not a finding");

    QString text = QStringLiteral("score %1 (%2): %3 for, %4 against")
               .arg(QString::number(score.score, 'f', 2),
                    statusLabel(score.suggested).toLower(), QString::number(score.supporting),
                    QString::number(score.contradicting));
    if (score.neutral > 0)
        text += QStringLiteral(", %1 neither").arg(score.neutral);
    if (score.supporting == 0)
        text += QStringLiteral(" -- nothing supports it");
    return text;
}

// ---------------------------------------------------------------------------
// Candidates from the data that is really there
// ---------------------------------------------------------------------------
QVector<HypothesisCandidate> candidatesFromData(const ProjectData &data)
{
    QVector<HypothesisCandidate> out;
    const Board *board = data.boards.isEmpty() ? nullptr : &data.boards.first();

    QSet<QString> netsDone;
    for (const Measurement &measurement : data.measurements) {
        // Only a reading that was judged can point anywhere. "Unknown" means the
        // data to judge it is missing, and a gap is not evidence.
        if (measurement.status != MeasurementStatus::Fail)
            continue;

        const TestPoint *point = findTestPoint(data, measurement.testPointUid);
        const QString identifier =
            point != nullptr ? point->identifier : QStringLiteral("(test point removed)");
        const QString explanation = measurementExplanation(measurement);
        const QString netName = point != nullptr ? point->netName : QString();

        HypothesisCandidate candidate;
        candidate.reason = QStringLiteral("%1: %2").arg(identifier, explanation);

        if (!netName.isEmpty() && !netsDone.contains(netName)) {
            netsDone.insert(netName);
            candidate.description =
                QStringLiteral("the fault is on net %1").arg(netName);
            candidate.componentUids = componentsOnNet(board, netName);
            candidate.prior = 0.5;
            out.append(candidate);

            // The parts on a net that is out of range, each one a separate
            // candidate and each one saying how weak on its own it is.
            const QStringList parts = candidate.componentUids;
            if (parts.size() > 1 && parts.size() <= 6) {
                for (const QString &componentUid : parts) {
                    HypothesisCandidate part;
                    const QString reference = referenceOf(board, componentUid);
                    part.description =
                        QStringLiteral("%1 on net %2 is faulty")
                            .arg(reference.isEmpty() ? QStringLiteral("a component") : reference,
                                 netName);
                    part.componentUids = QStringList{ componentUid };
                    part.reason =
                        QStringLiteral("it is one of %1 components on net %2, where %3")
                            .arg(QString::number(parts.size()), netName, candidate.reason);
                    part.prior = 0.5 / parts.size();
                    out.append(part);
                }
            }
            continue;
        } else if (netName.isEmpty()) {
            candidate.description =
                QStringLiteral("the fault is at %1").arg(identifier);
            if (point != nullptr && !point->componentUid.isEmpty())
                candidate.componentUids = QStringList{ point->componentUid };
            candidate.prior = 0.4;
        } else {
            continue;   // this net already has a candidate
        }
        out.append(candidate);
    }

    // A reading that was taken but could not be judged is not a candidate for
    // anything: it is a missing expected value, and saying so is more useful
    // than pretending it means something.
    return out;
}

QVector<SuggestedTest> suggestNextTests(const ProjectData &data,
                                        const QVector<HypothesisScore> &scores)
{
    QVector<SuggestedTest> out;
    const Board *board = data.boards.isEmpty() ? nullptr : &data.boards.first();

    QSet<QString> failingNets;
    for (const Measurement &measurement : data.measurements) {
        if (measurement.status != MeasurementStatus::Fail)
            continue;
        const TestPoint *point = findTestPoint(data, measurement.testPointUid);
        if (point != nullptr && !point->netName.isEmpty())
            failingNets.insert(point->netName);
    }

    for (const TestPoint &point : data.testPoints) {
        const Measurement latest = latestMeasurement(point.uid, data.measurements);
        SuggestedTest suggestion;
        suggestion.testPointUid = point.uid;
        suggestion.identifier = point.identifier;

        if (latest.uid.isEmpty()) {
            if (!point.netName.isEmpty() && failingNets.contains(point.netName)) {
                suggestion.information = 0.9;
                suggestion.reason =
                    QStringLiteral("never measured, and it sits on net %1 where a reading is "
                                   "already out of range")
                        .arg(point.netName);
            } else if (!point.componentUid.isEmpty()) {
                double best = 0.0;
                for (int i = 0; i < scores.size() && i < data.hypotheses.size(); ++i) {
                    if (!data.hypotheses.at(i).componentUids.contains(point.componentUid))
                        continue;
                    best = std::max(best, scores.at(i).score);
                }
                if (best > 0.0) {
                    const QString reference = referenceOf(board, point.componentUid);
                    suggestion.information = 0.3 + 0.6 * best;
                    suggestion.reason =
                        QStringLiteral("never measured, and it is on %1, which the current "
                                       "evidence points at (score %2)")
                            .arg(reference.isEmpty() ? QStringLiteral("a component") : reference,
                                 QString::number(best, 'f', 2));
                } else {
                    suggestion.information = 0.2;
                    suggestion.reason = QStringLiteral("never measured");
                }
            } else {
                suggestion.information = 0.2;
                suggestion.reason = QStringLiteral("never measured");
            }
        } else if (latest.status == MeasurementStatus::Unknown) {
            suggestion.information = 0.35;
            suggestion.reason =
                QStringLiteral("measured, but the reading proves nothing yet: %1")
                    .arg(measurementExplanation(latest));
        } else {
            continue;   // Pass or Fail: measured and judged, nothing to add
        }
        out.append(suggestion);
    }

    std::stable_sort(out.begin(), out.end(), [](const SuggestedTest &a, const SuggestedTest &b) {
        return a.information > b.information;
    });
    return out;
}

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------
QString startSession(Project *project, const QString &title, const QStringList &symptoms,
                     QString *error)
{
    if (project == nullptr) {
        if (error)
            *error = QStringLiteral("no project is open");
        return QString();
    }
    if (title.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("a session needs a title");
        return QString();
    }

    DiagnosticSession session;
    session.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.title = title.trimmed();
    session.started = QDateTime::currentDateTimeUtc();
    session.updated = session.started;
    session.symptoms = symptoms;
    project->data().sessions.append(session);
    project->markDirty();
    return session.uid;
}

bool closeSession(Project *project, const QString &uid, const QString &outcome, QString *error)
{
    DiagnosticSession *session = sessionByUid(project, uid);
    if (session == nullptr) {
        if (error)
            *error = QStringLiteral("no session with that id is open in this project");
        return false;
    }
    session->closed = true;
    session->outcome = outcome.trimmed();
    session->updated = QDateTime::currentDateTimeUtc();
    project->markDirty();
    return true;
}

DiagnosticSession *sessionByUid(Project *project, const QString &uid)
{
    if (project == nullptr || uid.isEmpty())
        return nullptr;
    for (DiagnosticSession &session : project->data().sessions) {
        if (session.uid == uid)
            return &session;
    }
    return nullptr;
}

DiagnosticSession *activeSession(Project *project)
{
    if (project == nullptr)
        return nullptr;
    DiagnosticSession *found = nullptr;
    for (DiagnosticSession &session : project->data().sessions) {
        if (session.closed)
            continue;
        if (found == nullptr || session.started > found->started)
            found = &session;
    }
    return found;
}

// ---------------------------------------------------------------------------
// Evidence
// ---------------------------------------------------------------------------
QString addEvidence(Project *project, const Evidence &evidence, QString *error)
{
    if (project == nullptr) {
        if (error)
            *error = QStringLiteral("no project is open");
        return QString();
    }
    if (evidence.hypothesisUid.isEmpty()) {
        if (error)
            *error = QStringLiteral("evidence has to belong to an hypothesis");
        return QString();
    }
    if (evidence.statement.trimmed().isEmpty() && evidence.reason.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("say what the evidence is, or why it counts");
        return QString();
    }

    Evidence stored = evidence;
    if (stored.uid.isEmpty())
        stored.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!stored.recorded.isValid())
        stored.recorded = QDateTime::currentDateTimeUtc();
    if (stored.provenance.status == VerificationStatus::Unknown
        && stored.kind == EvidenceKind::Measurement) {
        // A reading is measured data: it says so even when nobody filled in the
        // provenance by hand.
        stored.provenance.valueSource = ValueSource::Measured;
        stored.provenance.status = VerificationStatus::ExperimentallyVerified;
    }
    if (const DiagnosticSession *session = activeSession(project))
        stored.sessionId = session->uid;

    project->data().evidence.append(stored);
    project->markDirty();
    return stored.uid;
}

bool removeEvidence(Project *project, const QString &uid, QString *error)
{
    if (project == nullptr) {
        if (error)
            *error = QStringLiteral("no project is open");
        return false;
    }
    QVector<Evidence> &all = project->data().evidence;
    for (int i = 0; i < all.size(); ++i) {
        if (all.at(i).uid != uid)
            continue;
        all.remove(i);
        project->markDirty();
        return true;
    }
    if (error)
        *error = QStringLiteral("no evidence with that id is stored in this project");
    return false;
}

QVector<Evidence> evidenceFor(const QVector<Evidence> &evidence, const QString &hypothesisUid)
{
    QVector<Evidence> out;
    for (const Evidence &item : evidence) {
        if (item.hypothesisUid == hypothesisUid)
            out.append(item);
    }
    return out;
}

Evidence evidenceFromMeasurement(const Measurement &measurement, const TestPoint &point,
                                 EvidenceDirection direction, const QString &reason)
{
    Evidence evidence;
    evidence.kind = EvidenceKind::Measurement;
    evidence.direction = direction;
    evidence.refId = measurement.uid;
    evidence.recorded = measurement.timestamp;
    evidence.provenance.valueSource = ValueSource::Measured;
    evidence.provenance.status = VerificationStatus::ExperimentallyVerified;
    // A reading that proves nothing stays on record, but it is not evidence.
    evidence.inconclusive = measurement.status == MeasurementStatus::Unknown
                            || measurement.status == MeasurementStatus::NotTested;
    evidence.provenance.location =
        QStringLiteral("test point %1").arg(point.identifier.isEmpty() ? measurement.testPointUid
                                                                      : point.identifier);

    const QString reading = formatValue(measurement.value, measurement.unit);
    switch (measurement.status) {
    case MeasurementStatus::Pass:
        evidence.statement = QStringLiteral("%1 reads %2, which is inside the expected range")
                                 .arg(point.identifier, reading);
        break;
    case MeasurementStatus::Fail:
        evidence.statement = QStringLiteral("%1 reads %2, which is outside the expected range")
                                 .arg(point.identifier, reading);
        break;
    case MeasurementStatus::Unknown:
        evidence.statement = QStringLiteral("%1 reads %2, and the reading cannot be judged: %3")
                                 .arg(point.identifier, reading,
                                      measurementExplanation(measurement));
        break;
    case MeasurementStatus::NotTested:
        evidence.statement = QStringLiteral("%1 was never measured").arg(point.identifier);
        break;
    }
    evidence.reason = reason.trimmed().isEmpty() ? measurementExplanation(measurement)
                                                : reason.trimmed();
    return evidence;
}

QString sessionSummary(const ProjectData &data, const DiagnosticSession *session)
{
    const QVector<HypothesisScore> scores = scoreAll(data.hypotheses, data.evidence);
    int ruledOut = 0;
    int confirmed = 0;
    int withEvidence = 0;
    double best = 0.0;
    for (const HypothesisScore &score : scores) {
        if (score.supporting > 0 || score.contradicting > 0)
            ++withEvidence;
        if (score.suggested == HypothesisStatus::RuledOut)
            ++ruledOut;
        if (score.suggested == HypothesisStatus::ConfirmedByMeasurement
            || score.suggested == HypothesisStatus::ConfirmedByRepair) {
            ++confirmed;
        }
        best = std::max(best, score.score);
    }

    QString text;
    if (session != nullptr)
        text = QStringLiteral("%1: ").arg(session->title);
    text += QStringLiteral("%1 hypotheses, %2 with evidence, %3 ruled out, %4 confirmed")
                .arg(QString::number(data.hypotheses.size()), QString::number(withEvidence),
                     QString::number(ruledOut), QString::number(confirmed));
    if (data.hypotheses.isEmpty())
        text = QStringLiteral("no hypotheses recorded yet");
    else if (best > 0.0)
        text += QStringLiteral(", best score %1").arg(QString::number(best, 'f', 2));
    return text;
}

} // namespace eda
