// Bottom panel: hypotheses, the evidence for and against them, and what should
// be measured next.
//
// The panel shows three things the engine really computed, and nothing else:
// the score of each hypothesis with the sentence behind it, the evidence with
// what it weighs, and the test points that would tell us the most. A hypothesis
// without evidence stays "possible" and scores zero, and the panel says so
// rather than dressing it up.
#pragma once

#include <QWidget>

#include "../core/Diagnostics.h"
#include "../core/ProjectModel.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;

namespace eda {

class Project;

class DiagnosticsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

    void setProject(Project *project);
    // The component "Add..." proposes, usually the one selected elsewhere.
    void setContextComponent(const QString &uid);

    int visibleCount() const;                 // rows in the hypothesis list
    int evidenceCount() const;                // rows in the evidence list
    int suggestionCount() const;              // test points worth measuring next
    int candidateCount() const;               // what the data suggests so far

    QString selectedHypothesisUid() const;
    void selectHypothesis(const QString &uid);

    // Returns the uid of what was stored, or an empty string with <error> set.
    QString addHypothesis(const QString &description, const QStringList &componentUids = {},
                          QString *error = nullptr);
    QString addEvidence(const Evidence &evidence, QString *error = nullptr);
    bool removeHypothesis(const QString &uid, QString *error = nullptr);
    bool removeEvidence(const QString &uid, QString *error = nullptr);
    // Turn the candidates the data produced into hypotheses, by index.
    QStringList acceptCandidates(const QVector<int> &indices, QString *error = nullptr);

    QString startSession(const QString &title, const QStringList &symptoms = {},
                         QString *error = nullptr);
    bool closeSession(const QString &outcome, QString *error = nullptr);
    QString activeSessionUid() const;

    HypothesisScore scoreOf(const QString &hypothesisUid) const;
    QString summaryText() const;
    QString statusText() const;

public slots:
    // Re-reads the project: the panel keeps no copy of the data.
    void refresh();

signals:
    void hypothesesChanged();
    void activeSessionChanged(const QString &uid);

private slots:
    void onAdd();
    void onSuggest();
    void onEvidence();
    void onRemoveEvidence();
    void onRemove();
    void onStatusChanged(int index);
    void onSession();
    void onSelectionChanged();

private:
    const Hypothesis *hypothesisByUid(const QString &uid) const;
    void showDetail(const QString &hypothesisUid);
    QStringList componentChoice() const;

    Project *m_project = nullptr;
    QString m_contextComponent;
    QString m_selected;
    QVector<HypothesisScore> m_scores;

    QTreeWidget *m_tree = nullptr;          // "diagnostics.tree"
    QTreeWidget *m_reasons = nullptr;       // "diagnostics.reasons"
    QTreeWidget *m_evidence = nullptr;      // "diagnostics.evidence"
    QTreeWidget *m_next = nullptr;          // "diagnostics.next"
    QComboBox *m_status = nullptr;          // "diagnostics.status"
    QLabel *m_sessionLabel = nullptr;       // "diagnostics.session"
    QLabel *m_summary = nullptr;            // "diagnostics.summary"
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_evidenceButton = nullptr;
    QPushButton *m_removeEvidenceButton = nullptr;
};

} // namespace eda
