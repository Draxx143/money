#include "DiagnosticsPanel.h"

#include "../core/Logging.h"
#include "../core/Measurements.h"
#include "../core/Project.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace eda {
namespace {

const char *kColorConfirmed = "#00D1B2";
const char *kColorSupported = "#4DA3FF";
const char *kColorMuted = "#94A3B8";
const char *kColorDisagreement = "#FFB454";

QString statusColor(HypothesisStatus status)
{
    switch (status) {
    case HypothesisStatus::ConfirmedByMeasurement:
    case HypothesisStatus::ConfirmedByRepair:
        return QString::fromLatin1(kColorConfirmed);
    case HypothesisStatus::StronglySupported:
    case HypothesisStatus::Plausible:
        return QString::fromLatin1(kColorSupported);
    case HypothesisStatus::RuledOut:
    case HypothesisStatus::Possible:
    case HypothesisStatus::Unresolved:
        break;
    }
    return QString::fromLatin1(kColorMuted);
}

QString directionLabel(EvidenceDirection direction)
{
    switch (direction) {
    case EvidenceDirection::Supports: return QObject::tr("for");
    case EvidenceDirection::Contradicts: return QObject::tr("against");
    case EvidenceDirection::Neutral: break;
    }
    return QObject::tr("neither");
}

QString kindLabel(EvidenceKind kind)
{
    switch (kind) {
    case EvidenceKind::Measurement: return QObject::tr("measurement");
    case EvidenceKind::Observation: return QObject::tr("observation");
    case EvidenceKind::SourceDocument: return QObject::tr("source document");
    case EvidenceKind::Repair: return QObject::tr("repair");
    case EvidenceKind::PriorKnowledge: return QObject::tr("prior knowledge");
    case EvidenceKind::Unknown: break;
    }
    return QObject::tr("unspecified");
}

QString number(double value) { return QString::number(value, 'f', 2); }

// ---------------------------------------------------------------------------
// Dialog: one hypothesis
// ---------------------------------------------------------------------------
class HypothesisDialog : public QDialog
{
public:
    HypothesisDialog(const Board *board, const QStringList &selectedComponents,
                     QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("New hypothesis"));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_description = new QLineEdit(this);
        m_description->setPlaceholderText(tr("C1 is open circuit, the 5 V rail is shorted, ..."));
        form->addRow(tr("What might be wrong:"), m_description);

        m_components = new QListWidget(this);
        m_components->setSelectionMode(QAbstractItemView::MultiSelection);
        if (board != nullptr) {
            for (const Component &component : board->components) {
                const QString reference =
                    component.reference.isEmpty() ? component.uid : component.reference;
                const QString text = component.value.isEmpty()
                                         ? reference
                                         : QStringLiteral("%1 (%2)").arg(reference,
                                                                         component.value);
                auto *item = new QListWidgetItem(text, m_components);
                item->setData(Qt::UserRole, component.uid);
                if (selectedComponents.contains(component.uid))
                    item->setSelected(true);
            }
        }
        m_components->setFixedHeight(110);
        form->addRow(tr("Components involved:"), m_components);

        m_status = new QComboBox(this);
        m_status->addItem(tr("possible, nothing decided yet"),
                          eda::toString(HypothesisStatus::Possible));
        m_status->addItem(tr("plausible"), eda::toString(HypothesisStatus::Plausible));
        m_status->addItem(tr("strongly supported"),
                          eda::toString(HypothesisStatus::StronglySupported));
        m_status->addItem(tr("confirmed by a measurement"),
                          eda::toString(HypothesisStatus::ConfirmedByMeasurement));
        m_status->addItem(tr("confirmed by a repair"),
                          eda::toString(HypothesisStatus::ConfirmedByRepair));
        m_status->addItem(tr("ruled out"), eda::toString(HypothesisStatus::RuledOut));
        m_status->addItem(tr("unresolved"), eda::toString(HypothesisStatus::Unresolved));
        form->addRow(tr("Status:"), m_status);

        m_notes = new QPlainTextEdit(this);
        m_notes->setPlaceholderText(tr("what made you think of it, and what would disprove it"));
        m_notes->setFixedHeight(70);
        form->addRow(tr("Notes:"), m_notes);

        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_description, &QLineEdit::textChanged, this,
                [buttons](const QString &text) {
                    buttons->button(QDialogButtonBox::Ok)->setEnabled(!text.trimmed().isEmpty());
                });
        layout->addWidget(buttons);
    }

    QString description() const { return m_description->text().trimmed(); }
    QStringList components() const
    {
        QStringList uids;
        for (QListWidgetItem *item : m_components->selectedItems())
            uids.append(item->data(Qt::UserRole).toString());
        return uids;
    }
    HypothesisStatus status() const
    {
        return hypothesisStatusFromString(m_status->currentData().toString());
    }
    QString notes() const { return m_notes->toPlainText().trimmed(); }

private:
    QLineEdit *m_description = nullptr;
    QListWidget *m_components = nullptr;
    QComboBox *m_status = nullptr;
    QPlainTextEdit *m_notes = nullptr;
};

// ---------------------------------------------------------------------------
// Dialog: one piece of evidence
// ---------------------------------------------------------------------------
class EvidenceDialog : public QDialog
{
public:
    EvidenceDialog(const ProjectData &data, const QString &hypothesisDescription,
                   QWidget *parent = nullptr)
        : QDialog(parent)
        , m_data(data)
    {
        setWindowTitle(tr("Evidence"));
        auto *layout = new QVBoxLayout(this);

        auto *context = new QLabel(tr("For: %1").arg(hypothesisDescription), this);
        context->setWordWrap(true);
        context->setStyleSheet(QStringLiteral("color: #94A3B8;"));
        layout->addWidget(context);

        auto *form = new QFormLayout();

        m_reading = new QComboBox(this);
        m_reading->addItem(tr("(not a stored reading)"), QString());
        for (const Measurement &measurement : data.measurements) {
            const TestPoint *point = nullptr;
            for (const TestPoint &candidate : data.testPoints) {
                if (candidate.uid == measurement.testPointUid) {
                    point = &candidate;
                    break;
                }
            }
            const QString identifier =
                point != nullptr ? point->identifier : QStringLiteral("(removed test point)");
            m_reading->addItem(QStringLiteral("%1  %2: %3 (%4)")
                                   .arg(measurement.timestamp.toLocalTime().toString(
                                            QStringLiteral("yyyy-MM-dd hh:mm")),
                                        identifier, formatValue(measurement.value,
                                                                measurement.unit),
                                        eda::toString(measurement.status)),
                               measurement.uid);
        }
        form->addRow(tr("Stored reading:"), m_reading);

        m_kind = new QComboBox(this);
        m_kind->addItem(tr("a measurement"), eda::toString(EvidenceKind::Measurement));
        m_kind->addItem(tr("an observation on the bench"),
                        eda::toString(EvidenceKind::Observation));
        m_kind->addItem(tr("a source document"), eda::toString(EvidenceKind::SourceDocument));
        m_kind->addItem(tr("a repair"), eda::toString(EvidenceKind::Repair));
        m_kind->addItem(tr("prior knowledge"), eda::toString(EvidenceKind::PriorKnowledge));
        form->addRow(tr("What it is:"), m_kind);

        m_direction = new QComboBox(this);
        m_direction->addItem(tr("speaks for it"), eda::toString(EvidenceDirection::Supports));
        m_direction->addItem(tr("speaks against it"),
                             eda::toString(EvidenceDirection::Contradicts));
        m_direction->addItem(tr("neither"), eda::toString(EvidenceDirection::Neutral));
        form->addRow(tr("Which way:"), m_direction);

        m_statement = new QLineEdit(this);
        m_statement->setPlaceholderText(tr("the 5 V rail measures 0.4 ohm to ground"));
        form->addRow(tr("What it says:"), m_statement);

        m_reason = new QPlainTextEdit(this);
        m_reason->setPlaceholderText(tr("why this speaks for or against the hypothesis"));
        m_reason->setFixedHeight(60);
        form->addRow(tr("Why it counts:"), m_reason);

        m_source = new QLineEdit(this);
        m_source->setPlaceholderText(tr("datasheet, service manual, my notebook, ..."));
        form->addRow(tr("Source:"), m_source);

        m_trust = new QComboBox(this);
        m_trust->addItem(tr("confirmed by measurement"),
                         eda::toString(VerificationStatus::ExperimentallyVerified));
        m_trust->addItem(tr("checked by me"), eda::toString(VerificationStatus::UserVerified));
        m_trust->addItem(tr("from an official source"),
                         eda::toString(VerificationStatus::OfficialSourceVerified));
        m_trust->addItem(tr("copied from an imported file"),
                         eda::toString(VerificationStatus::ExtractedFromSource));
        m_trust->addItem(tr("inferred, not confirmed"),
                         eda::toString(VerificationStatus::Inferred));
        m_trust->addItem(tr("no source, it is only a note"),
                         eda::toString(VerificationStatus::Unverified));
        form->addRow(tr("How sure:"), m_trust);

        layout->addLayout(form);

        m_preview = new QLabel(this);
        m_preview->setWordWrap(true);
        m_preview->setStyleSheet(QStringLiteral("color: #94A3B8;"));
        layout->addWidget(m_preview);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        connect(m_reading, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &EvidenceDialog::applyReading);
        auto update = [this]() { updatePreview(); };
        connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update);
        connect(m_direction, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update);
        connect(m_trust, QOverload<int>::of(&QComboBox::currentIndexChanged), this, update);
        updatePreview();
    }

    Evidence evidence(const QString &hypothesisUid) const
    {
        Evidence item;
        item.hypothesisUid = hypothesisUid;
        item.kind = evidenceKindFromString(m_kind->currentData().toString());
        item.direction = evidenceDirectionFromString(m_direction->currentData().toString());
        item.statement = m_statement->text().trimmed();
        item.reason = m_reason->toPlainText().trimmed();
        item.refId = m_reading->currentData().toString();
        item.provenance.title = m_source->text().trimmed();
        item.provenance.status = verificationStatusFromString(m_trust->currentData().toString());
        if (item.kind == EvidenceKind::Measurement)
            item.provenance.valueSource = ValueSource::Measured;
        item.recorded = QDateTime::currentDateTimeUtc();
        return item;
    }

private:
    // A stored reading carries its own verdict and explanation: taking it over
    // is not a guess, it is the same sentence the measurement already says.
    void applyReading()
    {
        const QString uid = m_reading->currentData().toString();
        if (uid.isEmpty())
            return;
        for (const Measurement &measurement : m_data.measurements) {
            if (measurement.uid != uid)
                continue;
            TestPoint point;
            for (const TestPoint &candidate : m_data.testPoints) {
                if (candidate.uid == measurement.testPointUid) {
                    point = candidate;
                    break;
                }
            }
            // A reading that failed speaks for an hypothesis about a fault
            // there; one that passed speaks against it. The user can change it.
            const EvidenceDirection direction =
                measurement.status == MeasurementStatus::Pass ? EvidenceDirection::Contradicts
                                                             : EvidenceDirection::Supports;
            const Evidence taken = evidenceFromMeasurement(measurement, point, direction);
            m_kind->setCurrentIndex(m_kind->findData(eda::toString(taken.kind)));
            m_direction->setCurrentIndex(m_direction->findData(eda::toString(direction)));
            m_statement->setText(taken.statement);
            m_reason->setPlainText(taken.reason);
            m_trust->setCurrentIndex(
                m_trust->findData(eda::toString(VerificationStatus::ExperimentallyVerified)));
            break;
        }
        updatePreview();
    }

    void updatePreview()
    {
        Evidence preview = evidence(QString());
        const ScoredEvidence scored = scoreEvidence(preview);
        m_preview->setText(tr("It weighs %1 %2 the hypothesis. (%3)")
                               .arg(number(scored.weight), directionLabel(preview.direction),
                                    weightExplanation(preview)));
    }

    const ProjectData &m_data;
    QComboBox *m_reading = nullptr;
    QComboBox *m_kind = nullptr;
    QComboBox *m_direction = nullptr;
    QLineEdit *m_statement = nullptr;
    QPlainTextEdit *m_reason = nullptr;
    QLineEdit *m_source = nullptr;
    QComboBox *m_trust = nullptr;
    QLabel *m_preview = nullptr;
};

// ---------------------------------------------------------------------------
// Dialog: a session -- start one, or close the one that is open
// ---------------------------------------------------------------------------
class SessionDialog : public QDialog
{
public:
    SessionDialog(const QString &title, const QStringList &symptoms, bool closable,
                  QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(closable ? tr("Diagnostic session") : tr("New diagnostic session"));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_title = new QLineEdit(title, this);
        m_title->setPlaceholderText(tr("board dead on arrival, 5 V rail unstable, ..."));
        form->addRow(tr("What is being investigated:"), m_title);

        m_symptoms = new QPlainTextEdit(this);
        m_symptoms->setPlainText(symptoms.join(QStringLiteral("\n")));
        m_symptoms->setPlaceholderText(tr("one symptom per line: no 5 V, LED never lights"));
        m_symptoms->setFixedHeight(80);
        form->addRow(tr("Symptoms:"), m_symptoms);

        if (closable) {
            m_close = new QComboBox(this);
            m_close->addItem(tr("keep it open"), QString());
            m_close->addItem(tr("close it: the fault is fixed"), QStringLiteral("fixed"));
            m_close->addItem(tr("close it: the fault is still there"),
                             QStringLiteral("not fixed"));
            m_close->addItem(tr("close it: given up for now"), QStringLiteral("abandoned"));
            form->addRow(tr("Session:"), m_close);

            m_outcome = new QLineEdit(this);
            m_outcome->setPlaceholderText(tr("what was done, and what happened"));
            form->addRow(tr("Outcome:"), m_outcome);
        }

        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }

    QString title() const { return m_title->text().trimmed(); }
    QStringList symptoms() const
    {
        QStringList out;
        const QStringList lines = m_symptoms->toPlainText().split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.trimmed().isEmpty())
                out.append(line.trimmed());
        }
        return out;
    }
    bool closing() const
    {
        return m_close != nullptr && !m_close->currentData().toString().isEmpty();
    }
    QString outcome() const
    {
        if (m_close == nullptr)
            return QString();
        const QString kind = m_close->currentData().toString();
        const QString note = m_outcome != nullptr ? m_outcome->text().trimmed() : QString();
        if (kind.isEmpty())
            return QString();
        return note.isEmpty() ? kind : QStringLiteral("%1: %2").arg(kind, note);
    }

private:
    QLineEdit *m_title = nullptr;
    QPlainTextEdit *m_symptoms = nullptr;
    QComboBox *m_close = nullptr;
    QLineEdit *m_outcome = nullptr;
};

// ---------------------------------------------------------------------------
// Dialog: what the data suggests
// ---------------------------------------------------------------------------
class SuggestionDialog : public QDialog
{
public:
    SuggestionDialog(const QVector<HypothesisCandidate> &candidates, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("What the data suggests"));
        auto *layout = new QVBoxLayout(this);

        auto *intro = new QLabel(
            tr("These are not measurements and not a diagnosis. Each one is built from a "
               "reading in this project that came out of range, and it names the reading it "
               "came from. Untick what you do not want."),
            this);
        intro->setWordWrap(true);
        intro->setStyleSheet(QStringLiteral("color: #94A3B8;"));
        layout->addWidget(intro);

        m_tree = new QTreeWidget(this);
        m_tree->setColumnCount(3);
        m_tree->setHeaderLabels({ tr("Hypothesis"), tr("Worth"), tr("Because") });
        m_tree->setRootIsDecorated(false);
        m_tree->setAlternatingRowColors(true);
        m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        for (const HypothesisCandidate &candidate : candidates) {
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, candidate.description);
            item->setText(1, number(candidate.prior));
            item->setText(2, candidate.reason);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
        }
        layout->addWidget(m_tree, 1);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
        resize(760, 380);
    }

    QVector<int> selectedIndices() const
    {
        QVector<int> out;
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            if (m_tree->topLevelItem(i)->checkState(0) == Qt::Checked)
                out.append(i);
        }
        return out;
    }

private:
    QTreeWidget *m_tree = nullptr;
};

} // namespace

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    m_sessionLabel = new QLabel(tr("no session"), this);
    m_sessionLabel->setObjectName(QStringLiteral("diagnostics.session"));
    m_sessionLabel->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    topRow->addWidget(m_sessionLabel, 1);

    auto *sessionButton = new QPushButton(tr("Session..."), this);
    auto *addButton = new QPushButton(tr("Add..."), this);
    m_evidenceButton = new QPushButton(tr("Evidence..."), this);
    m_removeEvidenceButton = new QPushButton(tr("Remove evidence"), this);
    auto *suggestButton = new QPushButton(tr("Suggest..."), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    topRow->addWidget(sessionButton);
    topRow->addWidget(addButton);
    topRow->addWidget(m_evidenceButton);
    topRow->addWidget(m_removeEvidenceButton);
    topRow->addWidget(suggestButton);
    topRow->addWidget(m_removeButton);
    layout->addLayout(topRow);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("diagnostics.tree"));
    m_tree->setColumnCount(7);
    m_tree->setHeaderLabels({ tr("Hypothesis"), tr("Status"), tr("Suggested"), tr("Score"),
                              tr("For"), tr("Against"), tr("Components") });
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 2);

    auto *statusRow = new QHBoxLayout();
    statusRow->addWidget(new QLabel(tr("Recorded status:"), this));
    m_status = new QComboBox(this);
    m_status->setObjectName(QStringLiteral("diagnostics.status"));
    m_status->addItem(tr("possible, nothing decided yet"),
                      eda::toString(HypothesisStatus::Possible));
    m_status->addItem(tr("plausible"), eda::toString(HypothesisStatus::Plausible));
    m_status->addItem(tr("strongly supported"),
                      eda::toString(HypothesisStatus::StronglySupported));
    m_status->addItem(tr("confirmed by a measurement"),
                      eda::toString(HypothesisStatus::ConfirmedByMeasurement));
    m_status->addItem(tr("confirmed by a repair"),
                      eda::toString(HypothesisStatus::ConfirmedByRepair));
    m_status->addItem(tr("ruled out"), eda::toString(HypothesisStatus::RuledOut));
    m_status->addItem(tr("unresolved"), eda::toString(HypothesisStatus::Unresolved));
    statusRow->addWidget(m_status, 1);
    layout->addLayout(statusRow);

    // Three lists side by side: why it scores what it does, what was put
    // forward, and where to measure next.
    auto *columns = new QHBoxLayout();
    m_reasons = new QTreeWidget(this);
    m_reasons->setObjectName(QStringLiteral("diagnostics.reasons"));
    m_reasons->setColumnCount(1);
    m_reasons->setHeaderLabels({ tr("Why it scores what it does") });
    m_reasons->setRootIsDecorated(false);
    m_reasons->setAlternatingRowColors(true);

    m_evidence = new QTreeWidget(this);
    m_evidence->setObjectName(QStringLiteral("diagnostics.evidence"));
    m_evidence->setColumnCount(4);
    m_evidence->setHeaderLabels({ tr("Which way"), tr("What it is"), tr("Weight"),
                                  tr("Why it weighs that") });
    m_evidence->setRootIsDecorated(false);
    m_evidence->setAlternatingRowColors(true);
    m_evidence->setUniformRowHeights(true);

    m_next = new QTreeWidget(this);
    m_next->setObjectName(QStringLiteral("diagnostics.next"));
    m_next->setColumnCount(3);
    m_next->setHeaderLabels({ tr("Measure next"), tr("Worth"), tr("Because") });
    m_next->setRootIsDecorated(false);
    m_next->setAlternatingRowColors(true);
    m_next->setUniformRowHeights(true);

    columns->addWidget(m_reasons, 2);
    columns->addWidget(m_evidence, 3);
    columns->addWidget(m_next, 2);
    for (QTreeWidget *list : { m_reasons, m_evidence, m_next })
        list->setFixedHeight(120);
    layout->addLayout(columns);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("diagnostics.summary"));
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    layout->addWidget(m_summary);

    connect(sessionButton, &QPushButton::clicked, this, &DiagnosticsPanel::onSession);
    connect(addButton, &QPushButton::clicked, this, &DiagnosticsPanel::onAdd);
    connect(m_evidenceButton, &QPushButton::clicked, this, &DiagnosticsPanel::onEvidence);
    connect(m_removeEvidenceButton, &QPushButton::clicked, this,
            &DiagnosticsPanel::onRemoveEvidence);
    connect(suggestButton, &QPushButton::clicked, this, &DiagnosticsPanel::onSuggest);
    connect(m_removeButton, &QPushButton::clicked, this, &DiagnosticsPanel::onRemove);
    connect(m_status, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DiagnosticsPanel::onStatusChanged);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &DiagnosticsPanel::onSelectionChanged);
    connect(m_evidence, &QTreeWidget::itemSelectionChanged, this, [this]() {
        m_removeEvidenceButton->setEnabled(!m_evidence->selectedItems().isEmpty());
    });

    refresh();
}

void DiagnosticsPanel::setProject(Project *project)
{
    m_project = project;
    m_selected.clear();
    refresh();
}

void DiagnosticsPanel::setContextComponent(const QString &uid)
{
    m_contextComponent = uid;
}

int DiagnosticsPanel::visibleCount() const { return m_tree->topLevelItemCount(); }
int DiagnosticsPanel::evidenceCount() const { return m_evidence->topLevelItemCount(); }
int DiagnosticsPanel::suggestionCount() const { return m_next->topLevelItemCount(); }

int DiagnosticsPanel::candidateCount() const
{
    if (m_project == nullptr)
        return 0;
    return candidatesFromData(m_project->data()).size();
}

QString DiagnosticsPanel::selectedHypothesisUid() const { return m_selected; }

void DiagnosticsPanel::selectHypothesis(const QString &uid)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        if (m_tree->topLevelItem(i)->data(0, Qt::UserRole).toString() != uid)
            continue;
        m_tree->setCurrentItem(m_tree->topLevelItem(i));
        m_selected = uid;
        showDetail(uid);
        return;
    }
}

QString DiagnosticsPanel::addHypothesis(const QString &description,
                                        const QStringList &componentUids, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return QString();
    }
    if (description.trimmed().isEmpty()) {
        if (error)
            *error = tr("an hypothesis has to say what might be wrong");
        return QString();
    }

    Hypothesis hypothesis;
    hypothesis.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    hypothesis.description = description.trimmed();
    hypothesis.componentUids = componentUids;
    m_project->data().hypotheses.append(hypothesis);
    m_project->markDirty();
    m_selected = hypothesis.uid;
    logInfo(QStringLiteral("diagnostics"),
            QStringLiteral("hypothesis added: %1").arg(hypothesis.description));
    refresh();
    emit hypothesesChanged();
    return hypothesis.uid;
}

QString DiagnosticsPanel::addEvidence(const Evidence &evidence, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return QString();
    }
    const QString uid = eda::addEvidence(m_project, evidence, error);
    if (uid.isEmpty())
        return QString();
    logInfo(QStringLiteral("diagnostics"),
            QStringLiteral("evidence recorded (%1): %2")
                .arg(kindLabel(evidence.kind), evidence.statement));
    refresh();
    emit hypothesesChanged();
    return uid;
}

bool DiagnosticsPanel::removeHypothesis(const QString &uid, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return false;
    }
    QVector<Hypothesis> &all = m_project->data().hypotheses;
    for (int i = 0; i < all.size(); ++i) {
        if (all.at(i).uid != uid)
            continue;
        const QString description = all.at(i).description;
        all.remove(i);
        // Its evidence goes with it: evidence without an hypothesis means
        // nothing, and leaving orphans behind would only hide them.
        QVector<Evidence> &items = m_project->data().evidence;
        for (int j = items.size() - 1; j >= 0; --j) {
            if (items.at(j).hypothesisUid == uid)
                items.remove(j);
        }
        if (m_selected == uid)
            m_selected.clear();
        m_project->markDirty();
        logInfo(QStringLiteral("diagnostics"),
                QStringLiteral("hypothesis removed: %1").arg(description));
        refresh();
        emit hypothesesChanged();
        return true;
    }
    if (error)
        *error = tr("no hypothesis with that id is stored in this project");
    return false;
}

bool DiagnosticsPanel::removeEvidence(const QString &uid, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return false;
    }
    if (!eda::removeEvidence(m_project, uid, error))
        return false;
    logInfo(QStringLiteral("diagnostics"), QStringLiteral("evidence removed"));
    refresh();
    emit hypothesesChanged();
    return true;
}

QStringList DiagnosticsPanel::acceptCandidates(const QVector<int> &indices, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return QStringList();
    }
    const QVector<HypothesisCandidate> candidates = candidatesFromData(m_project->data());
    QStringList uids;
    for (int index : indices) {
        if (index < 0 || index >= candidates.size())
            continue;
        const HypothesisCandidate &candidate = candidates.at(index);
        Hypothesis hypothesis;
        hypothesis.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        hypothesis.description = candidate.description;
        hypothesis.componentUids = candidate.componentUids;
        hypothesis.status = HypothesisStatus::Possible;   // suggested, not concluded
        hypothesis.notes = tr("suggested from the data in this project: %1").arg(candidate.reason);
        m_project->data().hypotheses.append(hypothesis);
        uids.append(hypothesis.uid);
        logInfo(QStringLiteral("diagnostics"),
                QStringLiteral("hypothesis suggested: %1").arg(hypothesis.description));
    }
    if (uids.isEmpty()) {
        if (error)
            *error = tr("nothing was selected");
        return uids;
    }
    m_project->markDirty();
    m_selected = uids.first();
    refresh();
    emit hypothesesChanged();
    return uids;
}

QString DiagnosticsPanel::startSession(const QString &title, const QStringList &symptoms,
                                       QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return QString();
    }
    const QString uid = eda::startSession(m_project, title, symptoms, error);
    if (uid.isEmpty())
        return QString();
    logInfo(QStringLiteral("diagnostics"),
            QStringLiteral("session started: %1").arg(title.trimmed()));
    refresh();
    emit activeSessionChanged(uid);
    return uid;
}

bool DiagnosticsPanel::closeSession(const QString &outcome, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("no project is open");
        return false;
    }
    const DiagnosticSession *session = eda::activeSession(m_project);
    if (session == nullptr) {
        if (error)
            *error = tr("no session is open");
        return false;
    }
    const QString uid = session->uid;
    if (!eda::closeSession(m_project, uid, outcome, error))
        return false;
    logInfo(QStringLiteral("diagnostics"),
            QStringLiteral("session closed: %1").arg(outcome.trimmed()));
    refresh();
    emit activeSessionChanged(QString());
    return true;
}

QString DiagnosticsPanel::activeSessionUid() const
{
    const DiagnosticSession *session = eda::activeSession(m_project);
    return session != nullptr ? session->uid : QString();
}

HypothesisScore DiagnosticsPanel::scoreOf(const QString &hypothesisUid) const
{
    if (m_project == nullptr)
        return HypothesisScore();
    const QVector<Hypothesis> &all = m_project->data().hypotheses;
    for (int i = 0; i < all.size() && i < m_scores.size(); ++i) {
        if (all.at(i).uid == hypothesisUid)
            return m_scores.at(i);
    }
    return HypothesisScore();
}

QString DiagnosticsPanel::summaryText() const { return m_summary->text(); }
QString DiagnosticsPanel::statusText() const { return m_sessionLabel->text(); }

const Hypothesis *DiagnosticsPanel::hypothesisByUid(const QString &uid) const
{
    if (m_project == nullptr || uid.isEmpty())
        return nullptr;
    for (const Hypothesis &hypothesis : m_project->data().hypotheses) {
        if (hypothesis.uid == uid)
            return &hypothesis;
    }
    return nullptr;
}

void DiagnosticsPanel::refresh()
{
    m_tree->clear();
    m_reasons->clear();
    m_evidence->clear();
    m_next->clear();
    m_scores.clear();

    static const ProjectData empty;
    const ProjectData &data = m_project != nullptr ? m_project->data() : empty;

    m_scores = scoreAll(data.hypotheses, data.evidence);
    const Board *board = data.boards.isEmpty() ? nullptr : &data.boards.first();

    int row = 0;
    for (const Hypothesis &hypothesis : data.hypotheses) {
        const HypothesisScore score = row < m_scores.size() ? m_scores.at(row) : HypothesisScore();
        ++row;

        QStringList references;
        for (const QString &componentUid : hypothesis.componentUids) {
            const QString reference =
                board != nullptr && board->componentByUid(componentUid) != nullptr
                    ? board->componentByUid(componentUid)->reference
                    : QString();
            references.append(reference.isEmpty() ? componentUid : reference);
        }

        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, hypothesis.description);
        item->setText(1, eda::statusLabel(hypothesis.status));
        item->setText(2, eda::statusLabel(score.suggested));
        item->setText(3, number(score.score));
        item->setText(4, QString::number(score.supporting));
        item->setText(5, QString::number(score.contradicting));
        item->setText(6, references.join(QStringLiteral(", ")));
        item->setData(0, Qt::UserRole, hypothesis.uid);
        item->setForeground(1, QColor(statusColor(hypothesis.status)));
        item->setForeground(2, QColor(statusColor(score.suggested)));
        if (!hypothesis.notes.isEmpty())
            item->setToolTip(0, hypothesis.notes);

        // When the recorded status and what the evidence says disagree, the
        // panel shows it instead of quietly picking one.
        if (hypothesis.status != score.suggested) {
            item->setForeground(2, QColor(QString::fromLatin1(kColorDisagreement)));
            item->setToolTip(2, tr("the evidence suggests \"%1\", the recorded status is \"%2\"")
                                    .arg(eda::statusLabel(score.suggested),
                                         eda::statusLabel(hypothesis.status)));
        }
        m_tree->addTopLevelItem(item);
    }

    // What to measure next, always: it is the most useful thing on the panel
    // when nothing has been decided yet.
    for (const SuggestedTest &suggestion : suggestNextTests(data, m_scores)) {
        auto *item = new QTreeWidgetItem(m_next);
        item->setText(0, suggestion.identifier);
        item->setText(1, number(suggestion.information));
        item->setText(2, suggestion.reason);
        item->setData(0, Qt::UserRole, suggestion.testPointUid);
        m_next->addTopLevelItem(item);
    }

    const DiagnosticSession *session = eda::activeSession(m_project);
    if (session != nullptr) {
        m_sessionLabel->setText(tr("Session: %1 (%2)")
                                    .arg(session->title,
                                         session->symptoms.isEmpty()
                                             ? tr("open")
                                             : session->symptoms.join(QStringLiteral(", "))));
    } else {
        m_sessionLabel->setText(tr("no session"));
    }

    m_summary->setText(eda::sessionSummary(data, session));

    const bool hasSelection = !m_selected.isEmpty() && hypothesisByUid(m_selected) != nullptr;
    m_removeButton->setEnabled(hasSelection);
    m_evidenceButton->setEnabled(hasSelection);
    m_status->setEnabled(hasSelection);
    if (hasSelection) {
        showDetail(m_selected);
        // Keep the combo in step with what is stored, without re-entering here.
        const int index =
            m_status->findData(eda::toString(hypothesisByUid(m_selected)->status));
        if (index >= 0) {
            QSignalBlocker blocker(m_status);
            m_status->setCurrentIndex(index);
        }
    } else {
        m_status->setCurrentIndex(0);
    }
}

void DiagnosticsPanel::showDetail(const QString &hypothesisUid)
{
    m_reasons->clear();
    m_evidence->clear();

    const Hypothesis *hypothesis = hypothesisByUid(hypothesisUid);
    if (hypothesis == nullptr || m_project == nullptr)
        return;

    const HypothesisScore score = scoreOf(hypothesisUid);

    auto *headline = new QTreeWidgetItem(m_reasons);
    headline->setText(0, score.summary);
    headline->setForeground(0, QColor(statusColor(score.suggested)));
    m_reasons->addTopLevelItem(headline);
    for (const QString &reason : score.reasons) {
        auto *item = new QTreeWidgetItem(m_reasons);
        item->setText(0, reason);
        m_reasons->addTopLevelItem(item);
    }
    if (!hypothesis->notes.isEmpty()) {
        auto *item = new QTreeWidgetItem(m_reasons);
        item->setText(0, hypothesis->notes);
        item->setForeground(0, QColor(QString::fromLatin1(kColorMuted)));
        m_reasons->addTopLevelItem(item);
    }

    for (const ScoredEvidence &item : score.items) {
        auto *row = new QTreeWidgetItem(m_evidence);
        row->setText(0, directionLabel(item.evidence.direction));
        row->setText(1, item.evidence.statement);
        row->setText(2, number(item.weight));
        row->setText(3, weightExplanation(item.evidence));
        row->setData(0, Qt::UserRole, item.evidence.uid);
        if (item.evidence.inconclusive)
            row->setForeground(2, QColor(QString::fromLatin1(kColorDisagreement)));
        if (!item.evidence.reason.isEmpty())
            row->setToolTip(1, item.evidence.reason);
        m_evidence->addTopLevelItem(row);
    }
    m_removeEvidenceButton->setEnabled(!m_evidence->selectedItems().isEmpty());
}

QStringList DiagnosticsPanel::componentChoice() const
{
    return m_contextComponent.isEmpty() ? QStringList() : QStringList{ m_contextComponent };
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------
void DiagnosticsPanel::onAdd()
{
    if (m_project == nullptr) {
        QMessageBox::information(this, tr("No project"), tr("Open a project first."));
        return;
    }
    const Board *board = m_project->primaryBoard();
    HypothesisDialog dialog(board, componentChoice(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString uid = addHypothesis(dialog.description(), dialog.components());
    if (uid.isEmpty())
        return;
    for (Hypothesis &stored : m_project->data().hypotheses) {
        if (stored.uid != uid)
            continue;
        stored.status = dialog.status();
        stored.notes = dialog.notes();
        break;
    }
    m_project->markDirty();
    refresh();
    emit hypothesesChanged();
}

void DiagnosticsPanel::onSuggest()
{
    if (m_project == nullptr) {
        QMessageBox::information(this, tr("No project"), tr("Open a project first."));
        return;
    }
    const QVector<HypothesisCandidate> candidates = candidatesFromData(m_project->data());
    if (candidates.isEmpty()) {
        QMessageBox::information(
            this, tr("Nothing to suggest"),
            tr("No reading in this project came out of range, so there is nothing to point "
               "at. A candidate is only built from a stored reading that failed."));
        return;
    }

    SuggestionDialog dialog(candidates, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString error;
    if (acceptCandidates(dialog.selectedIndices(), &error).isEmpty() && !error.isEmpty())
        QMessageBox::warning(this, tr("Nothing added"), error);
}

void DiagnosticsPanel::onEvidence()
{
    if (m_project == nullptr || hypothesisByUid(m_selected) == nullptr)
        return;

    EvidenceDialog dialog(m_project->data(), hypothesisByUid(m_selected)->description, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    if (addEvidence(dialog.evidence(m_selected), &error).isEmpty())
        QMessageBox::warning(this, tr("Not recorded"), error);
}

void DiagnosticsPanel::onRemoveEvidence()
{
    const QList<QTreeWidgetItem *> rows = m_evidence->selectedItems();
    if (rows.isEmpty() || m_project == nullptr)
        return;
    const QString uid = rows.first()->data(0, Qt::UserRole).toString();
    const QString statement = rows.first()->text(1);
    if (QMessageBox::question(this, tr("Remove evidence"),
                              tr("Remove this piece of evidence?\n\n%1").arg(statement))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!removeEvidence(uid, &error))
        QMessageBox::warning(this, tr("Not removed"), error);
}

void DiagnosticsPanel::onRemove()
{
    const QString uid = m_selected;
    const Hypothesis *hypothesis = hypothesisByUid(uid);
    if (m_project == nullptr || hypothesis == nullptr)
        return;
    if (QMessageBox::question(
            this, tr("Remove hypothesis"),
            tr("Remove this hypothesis and its evidence?\n\n%1").arg(hypothesis->description))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!removeHypothesis(uid, &error))
        QMessageBox::warning(this, tr("Not removed"), error);
}

void DiagnosticsPanel::onStatusChanged(int index)
{
    if (index < 0 || m_project == nullptr || m_selected.isEmpty())
        return;
    Hypothesis *hypothesis = nullptr;
    for (Hypothesis &candidate : m_project->data().hypotheses) {
        if (candidate.uid == m_selected) {
            hypothesis = &candidate;
            break;
        }
    }
    if (hypothesis == nullptr)
        return;
    const HypothesisStatus status =
        hypothesisStatusFromString(m_status->itemData(index).toString());
    if (hypothesis->status == status)
        return;
    hypothesis->status = status;
    m_project->markDirty();
    logInfo(QStringLiteral("diagnostics"),
            QStringLiteral("status recorded: %1 -- %2")
                .arg(hypothesis->description, eda::statusLabel(status)));
    refresh();
    emit hypothesesChanged();
}

void DiagnosticsPanel::onSession()
{
    if (m_project == nullptr) {
        QMessageBox::information(this, tr("No project"), tr("Open a project first."));
        return;
    }
    const DiagnosticSession *session = eda::activeSession(m_project);
    SessionDialog dialog(session != nullptr ? session->title : QString(),
                         session != nullptr ? session->symptoms : QStringList(),
                         session != nullptr, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    if (session != nullptr) {
        if (dialog.closing() && !closeSession(dialog.outcome(), &error))
            QMessageBox::warning(this, tr("Session not closed"), error);
        return;
    }
    if (startSession(dialog.title(), dialog.symptoms(), &error).isEmpty())
        QMessageBox::warning(this, tr("Session not started"), error);
}

void DiagnosticsPanel::onSelectionChanged()
{
    const QList<QTreeWidgetItem *> rows = m_tree->selectedItems();
    m_selected = rows.isEmpty() ? QString() : rows.first()->data(0, Qt::UserRole).toString();
    const bool hasSelection = !m_selected.isEmpty();
    m_removeButton->setEnabled(hasSelection);
    m_evidenceButton->setEnabled(hasSelection);
    m_status->setEnabled(hasSelection);
    if (hasSelection) {
        const Hypothesis *hypothesis = hypothesisByUid(m_selected);
        m_status->setEnabled(hypothesis != nullptr);
        if (hypothesis != nullptr) {
            const int index = m_status->findData(eda::toString(hypothesis->status));
            if (index >= 0) {
                QSignalBlocker blocker(m_status);
                m_status->setCurrentIndex(index);
            }
        }
    }
    showDetail(m_selected);
}

} // namespace eda
