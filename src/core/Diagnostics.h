// The diagnostic engine: hypotheses, the evidence for and against them, and
// what should be measured next.
//
// The rules this file obeys, because a diagnostic tool that flatters the user
// is worse than no tool at all:
//
//   * Nothing is inferred from nothing. An hypothesis without evidence is
//     "possible" and scores zero; it is never called plausible because it was
//     written first or because it sounds likely.
//   * Every weight is explained. The panel shows the number and the sentence
//     that produced it, so a user can disagree with the engine.
//   * Suggestions are made from data that is really in the project - a stored
//     reading, a net, a component - and each one names the data it came from.
//   * A reading that cannot be judged (no expected value, or a unit that means
//     something else) is not evidence for anything. It is a gap, and it is
//     reported as a gap.
#pragma once

#include "ProjectModel.h"

#include <QVector>
#include <QString>
#include <QStringList>

namespace eda {

class Project;

// ---------------------------------------------------------------------------
// Weights
// ---------------------------------------------------------------------------
// How much one piece of evidence counts, and why. The weight is a property of
// the *kind* of evidence and of how trustworthy it is - never of the order it
// was entered in.
struct ScoredEvidence
{
    Evidence evidence;
    double weight = 0.0;
    QString explanation;      // the sentence behind the number
};

ScoredEvidence scoreEvidence(const Evidence &evidence);
double weightOf(const Evidence &evidence);
QString weightExplanation(const Evidence &evidence);

// ---------------------------------------------------------------------------
// Scoring one hypothesis
// ---------------------------------------------------------------------------
struct HypothesisScore
{
    double score = 0.0;           // 0..1: how much the evidence supports it
    double support = 0.0;         // summed weight of what speaks for it
    double contradiction = 0.0;   // summed weight of what speaks against it
    int supporting = 0;
    int contradicting = 0;
    int neutral = 0;

    QVector<ScoredEvidence> items;   // newest first
    QStringList reasons;             // one sentence per item, newest first
    HypothesisStatus suggested = HypothesisStatus::Possible;
    QString summary;                 // one honest sentence about the whole thing
};

HypothesisScore scoreHypothesis(const Hypothesis &hypothesis,
                                const QVector<Evidence> &evidence);
QVector<HypothesisScore> scoreAll(const QVector<Hypothesis> &hypotheses,
                                  const QVector<Evidence> &evidence);

// The status the evidence justifies. It is a suggestion: the user always has
// the last word, and a manual status is never silently overwritten.
HypothesisStatus statusFromScore(const HypothesisScore &score);
QString scoreSummary(const HypothesisScore &score);
QString statusLabel(HypothesisStatus status);

// ---------------------------------------------------------------------------
// Candidates built from what the project really contains
// ---------------------------------------------------------------------------
struct HypothesisCandidate
{
    QString description;
    QStringList componentUids;
    QString reason;        // which stored data produced this
    double prior = 0.0;    // 0..1, how much the data alone suggests it
};

QVector<HypothesisCandidate> candidatesFromData(const ProjectData &data);

// Which test points would tell us the most. Ranked, each with the reason.
struct SuggestedTest
{
    QString testPointUid;
    QString identifier;
    QString reason;
    double information = 0.0;   // 0..1
};

QVector<SuggestedTest> suggestNextTests(const ProjectData &data,
                                        const QVector<HypothesisScore> &scores = {});

// ---------------------------------------------------------------------------
// Sessions
// ---------------------------------------------------------------------------
// Returns the uid of the new session, or an empty string with <error> set.
QString startSession(Project *project, const QString &title, const QStringList &symptoms,
                     QString *error = nullptr);
bool closeSession(Project *project, const QString &uid, const QString &outcome,
                  QString *error = nullptr);
DiagnosticSession *sessionByUid(Project *project, const QString &uid);
// The newest session that is still open, or nullptr when there is none.
DiagnosticSession *activeSession(Project *project);
const DiagnosticSession *activeSession(const Project *project);

// ---------------------------------------------------------------------------
// Evidence
// ---------------------------------------------------------------------------
QString addEvidence(Project *project, const Evidence &evidence, QString *error = nullptr);
bool removeEvidence(Project *project, const QString &uid, QString *error = nullptr);
QVector<Evidence> evidenceFor(const QVector<Evidence> &evidence, const QString &hypothesisUid);

// A stored reading becomes evidence: the verdict and the explanation are the
// ones the measurement already carries, so a "Fail" supports an hypothesis
// about that net and contradicts one that says the net is healthy.
Evidence evidenceFromMeasurement(const Measurement &measurement, const TestPoint &point,
                                 EvidenceDirection direction = EvidenceDirection::Supports,
                                 const QString &reason = QString());

// A short, honest description of where the diagnosis stands. Used by the panel
// and later by the report.
QString sessionSummary(const ProjectData &data, const DiagnosticSession *session = nullptr);

} // namespace eda
