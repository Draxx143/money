#include "TestPointPanel.h"

#include "../core/Logging.h"
#include "../core/Project.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace eda {
namespace {

const char *kStatusColorPass = "#00D1B2";
const char *kStatusColorFail = "#FF6B6B";
const char *kStatusColorMuted = "#94A3B8";

// 0.250 -> "0.25": the panel shows numbers the way a person would write them.
QString trimNumber(double value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

QString statusLabel(MeasurementStatus status)
{
    switch (status) {
    case MeasurementStatus::NotTested: return QObject::tr("Not tested");
    case MeasurementStatus::Pass: return QObject::tr("Pass");
    case MeasurementStatus::Fail: return QObject::tr("Fail");
    case MeasurementStatus::Unknown: break;
    }
    return QObject::tr("Unknown");
}

QString statusColor(MeasurementStatus status)
{
    switch (status) {
    case MeasurementStatus::Pass:
        return QString::fromLatin1(kStatusColorPass);
    case MeasurementStatus::Fail:
        return QString::fromLatin1(kStatusColorFail);
    case MeasurementStatus::NotTested:
    case MeasurementStatus::Unknown:
        break;
    }
    return QString::fromLatin1(kStatusColorMuted);
}

QStringList testPointCategories()
{
    return { QStringLiteral("voltage"), QStringLiteral("current"), QStringLiteral("resistance"),
             QStringLiteral("continuity"), QStringLiteral("diode"), QStringLiteral("capacitance"),
             QStringLiteral("frequency"), QStringLiteral("logic"), QStringLiteral("temperature"),
             QStringLiteral("signal"), QStringLiteral("other") };
}

QStringList unitSuggestions(const QString &category)
{
    if (category == QStringLiteral("voltage"))
        return { QStringLiteral("V"), QStringLiteral("mV") };
    if (category == QStringLiteral("current"))
        return { QStringLiteral("A"), QStringLiteral("mA"), QStringLiteral("uA") };
    if (category == QStringLiteral("resistance") || category == QStringLiteral("continuity"))
        return { QStringLiteral("ohm"), QStringLiteral("kohm"), QStringLiteral("Mohm") };
    if (category == QStringLiteral("capacitance"))
        return { QStringLiteral("F"), QStringLiteral("uF"), QStringLiteral("nF"),
                 QStringLiteral("pF") };
    if (category == QStringLiteral("frequency"))
        return { QStringLiteral("Hz"), QStringLiteral("kHz"), QStringLiteral("MHz") };
    if (category == QStringLiteral("temperature"))
        return { QStringLiteral("degC") };
    return { QStringLiteral("V"), QStringLiteral("A"), QStringLiteral("ohm"), QStringLiteral("Hz"),
             QStringLiteral("degC") };
}

// A value that can also be left empty, because "we do not know what it should
// be" is a different thing from "it should be zero".
class OptionalValue : public QWidget
{
public:
    explicit OptionalValue(const QStringList &units, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_enabled = new QCheckBox(tr("expected"), this);
        m_spin = new QDoubleSpinBox(this);
        m_spin->setRange(-1e9, 1e9);
        m_spin->setDecimals(4);
        m_spin->setEnabled(false);
        m_unit = new QComboBox(this);
        m_unit->setEditable(true);
        m_unit->addItems(units);
        m_unit->setEnabled(false);
        m_tolerance = new QDoubleSpinBox(this);
        m_tolerance->setRange(0.0, 1e9);
        m_tolerance->setDecimals(4);
        m_tolerance->setEnabled(false);
        m_tolerance->setPrefix(QStringLiteral("+- "));
        m_percent = new QCheckBox(tr("%"), this);
        m_percent->setEnabled(false);

        layout->addWidget(m_enabled);
        layout->addWidget(m_spin, 2);
        layout->addWidget(m_unit, 1);
        layout->addWidget(m_tolerance, 1);
        layout->addWidget(m_percent);

        connect(m_enabled, &QCheckBox::toggled, this, [this](bool on) {
            m_spin->setEnabled(on);
            m_unit->setEnabled(on);
            m_tolerance->setEnabled(on);
            m_percent->setEnabled(on);
        });
    }

    bool hasValue() const { return m_enabled->isChecked(); }
    double value() const { return m_spin->value(); }
    QString unit() const { return m_unit->currentText().trimmed(); }
    double tolerance() const { return m_tolerance->value(); }
    bool hasTolerance() const { return m_enabled->isChecked() && m_tolerance->value() > 0.0; }
    bool percent() const { return m_percent->isChecked(); }

    void setUnits(const QStringList &units)
    {
        const QString current = m_unit->currentText();
        m_unit->clear();
        m_unit->addItems(units);
        if (!current.isEmpty())
            m_unit->setCurrentText(current);
    }

    void setValue(double value, const QString &unit, double tolerance, bool isPercent)
    {
        m_enabled->setChecked(true);
        m_spin->setValue(value);
        m_unit->setCurrentText(unit);
        m_tolerance->setValue(tolerance);
        m_percent->setChecked(isPercent);
    }

private:
    QCheckBox *m_enabled = nullptr;
    QDoubleSpinBox *m_spin = nullptr;
    QComboBox *m_unit = nullptr;
    QDoubleSpinBox *m_tolerance = nullptr;
    QCheckBox *m_percent = nullptr;
};

// Dialog for a new test point.
class TestPointDialog : public QDialog
{
public:
    TestPointDialog(const Board *board, const QString &componentUid, const QString &identifier,
                    QWidget *parent = nullptr)
        : QDialog(parent)
        , m_board(board)
    {
        setWindowTitle(tr("New test point"));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_identifier = new QLineEdit(identifier, this);
        form->addRow(tr("Identifier:"), m_identifier);

        m_component = new QComboBox(this);
        m_component->addItem(tr("(a bare pad or net)"), QString());
        if (board != nullptr) {
            for (const Component &component : board->components) {
                const QString label = component.reference.isEmpty() ? component.uid
                                                                   : component.reference;
                const QString text = component.value.isEmpty()
                                         ? label
                                         : QStringLiteral("%1 (%2)").arg(label, component.value);
                m_component->addItem(text, component.uid);
            }
        }
        form->addRow(tr("Component:"), m_component);

        m_pad = new QComboBox(this);
        m_pad->addItem(tr("(not a specific pad)"), QString());
        form->addRow(tr("Pin / pad:"), m_pad);

        m_net = new QLineEdit(this);
        m_net->setPlaceholderText(tr("the net it belongs to, if it is known"));
        form->addRow(tr("Net:"), m_net);

        m_category = new QComboBox(this);
        m_category->setEditable(true);
        m_category->addItems(testPointCategories());
        form->addRow(tr("What is measured:"), m_category);

        m_expected = new OptionalValue(unitSuggestions(m_category->currentText()), this);
        form->addRow(tr("Expected:"), m_expected);

        m_conditions = new QLineEdit(this);
        m_conditions->setPlaceholderText(tr("board powered, 24 V applied, relay off"));
        form->addRow(tr("Test conditions:"), m_conditions);

        m_instrument = new QLineEdit(this);
        m_instrument->setPlaceholderText(tr("Fluke 87V, DS1054Z, ..."));
        form->addRow(tr("Instrument:"), m_instrument);

        m_probes = new QLineEdit(this);
        m_probes->setPlaceholderText(tr("where to put the probes"));
        form->addRow(tr("Probe notes:"), m_probes);

        form->addRow(new QLabel(tr("Where does the expected value come from?"), this));
        m_source = new QLineEdit(this);
        m_source->setPlaceholderText(tr("datasheet, schematic note, a working board, ..."));
        form->addRow(tr("Source:"), m_source);
        m_location = new QLineEdit(this);
        m_location->setPlaceholderText(tr("page 12, section 6.3"));
        form->addRow(tr("Location:"), m_location);
        m_status = new QComboBox(this);
        m_status->addItem(tr("from an official document"),
                          eda::toString(VerificationStatus::OfficialSourceVerified));
        m_status->addItem(tr("measured on a known good board"),
                          eda::toString(VerificationStatus::ExperimentallyVerified));
        m_status->addItem(tr("my own note, not confirmed"),
                          eda::toString(VerificationStatus::Unverified));
        m_status->addItem(tr("inferred from the circuit"),
                          eda::toString(VerificationStatus::Inferred));
        form->addRow(tr("Trust:"), m_status);

        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        connect(m_component, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &TestPointDialog::fillPads);
        connect(m_pad, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &TestPointDialog::fillNet);
        connect(m_category, &QComboBox::currentTextChanged, this, [this](const QString &category) {
            m_expected->setUnits(unitSuggestions(category));
        });

        if (!componentUid.isEmpty()) {
            const int index = m_component->findData(componentUid);
            if (index >= 0)
                m_component->setCurrentIndex(index);
        }
        fillPads();
    }

    TestPoint testPoint(const QString &uid) const
    {
        TestPoint point;
        point.uid = uid;
        point.identifier = m_identifier->text().trimmed();
        point.componentUid = m_component->currentData().toString();
        point.pinOrPad = m_pad->currentData().toString();
        point.netName = m_net->text().trimmed();
        point.category = m_category->currentText().trimmed();
        point.expectedValue = m_expected->value();
        point.hasExpectedValue = m_expected->hasValue();
        point.tolerance = m_expected->tolerance();
        point.hasTolerance = m_expected->hasTolerance();
        point.toleranceIsPercent = m_expected->percent();
        point.unit = m_expected->unit();
        point.testConditions = m_conditions->text().trimmed();
        point.instrument = m_instrument->text().trimmed();
        point.probeNotes = m_probes->text().trimmed();

        if (m_expected->hasValue()) {
            point.provenance.title = m_source->text().trimmed();
            point.provenance.location = m_location->text().trimmed();
            point.provenance.status =
                verificationStatusFromString(m_status->currentData().toString());
            point.provenance.valueSource =
                point.provenance.title.isEmpty() ? ValueSource::ManualEntry
                                                 : ValueSource::ExtractedFromSource;
            point.provenance.retrieved = QDateTime::currentDateTimeUtc();
            if (point.provenance.title.isEmpty())
                point.provenance.notes = QStringLiteral("no source was given");
        }
        return point;
    }

private:
    void fillPads()
    {
        m_pad->clear();
        m_pad->addItem(tr("(not a specific pad)"), QString());
        const QString uid = m_component->currentData().toString();
        const Component *component = m_board != nullptr ? m_board->componentByUid(uid) : nullptr;
        if (component == nullptr)
            return;
        for (const Pad &pad : component->pads) {
            const QString label = pad.pin.isEmpty() ? pad.id : pad.pin;
            const QString text = pad.netName.isEmpty()
                                     ? label
                                     : QStringLiteral("%1 (%2)").arg(label, pad.netName);
            m_pad->addItem(text, label);
        }
    }
    void fillNet()
    {
        if (!m_net->text().isEmpty())
            return;
        const QString text = m_pad->currentText();
        const int start = text.indexOf(QLatin1Char('('));
        if (start < 0) {
            m_net->clear();
            return;
        }
        m_net->setText(text.mid(start + 1).section(QStringLiteral(")"), 0, 0).trimmed());
    }

    const Board *m_board = nullptr;
    QLineEdit *m_identifier = nullptr;
    QComboBox *m_component = nullptr;
    QComboBox *m_pad = nullptr;
    QLineEdit *m_net = nullptr;
    QComboBox *m_category = nullptr;
    OptionalValue *m_expected = nullptr;
    QLineEdit *m_conditions = nullptr;
    QLineEdit *m_instrument = nullptr;
    QLineEdit *m_probes = nullptr;
    QLineEdit *m_source = nullptr;
    QLineEdit *m_location = nullptr;
    QComboBox *m_status = nullptr;
};

// Dialog for one reading. It shows the verdict while the value is typed, so a
// measurement is never stored as a surprise.
class MeasurementDialog : public QDialog
{
public:
    MeasurementDialog(const TestPoint &point, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_point(point)
    {
        setWindowTitle(tr("Measure %1").arg(point.identifier));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_expectation = new QLabel(this);
        m_expectation->setWordWrap(true);
        m_expectation->setStyleSheet(QStringLiteral("color: #94A3B8;"));
        form->addRow(tr("Expected:"), m_expectation);

        m_value = new QDoubleSpinBox(this);
        m_value->setRange(-1e9, 1e9);
        m_value->setDecimals(6);
        m_value->setKeyboardTracking(false);
        form->addRow(tr("Measured:"), m_value);

        m_unit = new QComboBox(this);
        m_unit->setEditable(true);
        m_unit->addItems(unitSuggestions(point.category));
        m_unit->setCurrentText(point.unit);
        form->addRow(tr("Unit:"), m_unit);

        m_instrument = new QLineEdit(point.instrument, this);
        form->addRow(tr("Instrument:"), m_instrument);

        m_conditions = new QLineEdit(point.testConditions, this);
        form->addRow(tr("Conditions:"), m_conditions);

        m_notes = new QPlainTextEdit(this);
        m_notes->setFixedHeight(50);
        form->addRow(tr("Notes:"), m_notes);

        m_verdict = new QLabel(this);
        m_verdict->setWordWrap(true);
        m_verdict->setStyleSheet(QStringLiteral("color: #94A3B8;"));
        form->addRow(tr("Verdict:"), m_verdict);

        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);

        connect(m_value, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                &MeasurementDialog::updateVerdict);
        connect(m_unit, &QComboBox::currentTextChanged, this, &MeasurementDialog::updateVerdict);

        updateVerdict();
    }

    double value() const { return m_value->value(); }
    QString unit() const { return m_unit->currentText().trimmed(); }
    QString instrument() const { return m_instrument->text().trimmed(); }
    QString conditions() const { return m_conditions->text().trimmed(); }
    QString notes() const { return m_notes->toPlainText().trimmed(); }

private:
    void updateVerdict()
    {
        const QString expectation =
            m_point.hasExpectedValue
                ? tr("%1%2 (source: %3)")
                      .arg(formatValue(m_point.expectedValue, m_point.unit))
                      .arg(m_point.hasTolerance
                               ? tr(", tolerance %1%2")
                                     .arg(m_point.tolerance, 0, 'f', 3)
                                     .arg(m_point.toleranceIsPercent ? QStringLiteral(" %")
                                                                     : QString())
                               : QString())
                      .arg(m_point.provenance.title.isEmpty() ? tr("not recorded")
                                                             : m_point.provenance.title)
                : tr("nothing: no expected value was recorded, so this reading cannot pass "
                     "or fail");
        m_expectation->setText(expectation);

        Tolerance tolerance;
        tolerance.value = m_point.tolerance;
        tolerance.isPercent = m_point.toleranceIsPercent;
        tolerance.hasValue = m_point.hasTolerance;
        QString explanation;
        const MeasurementStatus status =
            evaluateMeasurement(value(), m_point.expectedValue, m_point.hasExpectedValue, tolerance,
                                unit(), m_point.unit, &explanation);
        m_verdict->setText(QStringLiteral("%1 -- %2").arg(statusLabel(status), explanation));
        m_verdict->setStyleSheet(QStringLiteral("color: %1;").arg(statusColor(status)));
    }

    TestPoint m_point;
    QLabel *m_expectation = nullptr;
    QDoubleSpinBox *m_value = nullptr;
    QComboBox *m_unit = nullptr;
    QLineEdit *m_instrument = nullptr;
    QLineEdit *m_conditions = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QLabel *m_verdict = nullptr;
};

} // namespace

TestPointPanel::TestPointPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *buttonRow = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add..."), this);
    m_measureButton = new QPushButton(tr("Measure..."), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(m_measureButton);
    buttonRow->addWidget(m_removeButton);

    m_filter = new QComboBox(this);
    m_filter->addItem(tr("All"), QString());
    m_filter->addItem(tr("Not tested"), QStringLiteral("untested"));
    m_filter->addItem(tr("Failing"), QStringLiteral("failing"));
    m_filter->addItem(tr("Passing"), QStringLiteral("passing"));
    m_filter->addItem(tr("Without a verdict"), QStringLiteral("unknown"));
    buttonRow->addWidget(m_filter);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("testpoints.tree"));
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({ tr("Test point"), tr("Component"), tr("Net"), tr("Expected"),
                              tr("Last reading"), tr("Verdict") });
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);

    m_measurements = new QTreeWidget(this);
    m_measurements->setObjectName(QStringLiteral("testpoints.measurements"));
    m_measurements->setColumnCount(4);
    m_measurements->setHeaderLabels({ tr("When"), tr("Value"), tr("Verdict"), tr("Instrument") });
    m_measurements->setRootIsDecorated(false);
    m_measurements->setAlternatingRowColors(true);
    m_measurements->setUniformRowHeights(true);
    m_measurements->setFixedHeight(110);
    layout->addWidget(m_measurements);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    layout->addWidget(m_status);

    connect(addButton, &QPushButton::clicked, this, &TestPointPanel::onAdd);
    connect(m_measureButton, &QPushButton::clicked, this, &TestPointPanel::onMeasure);
    connect(m_removeButton, &QPushButton::clicked, this, &TestPointPanel::onRemove);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &TestPointPanel::onSelectionChanged);
    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestPointPanel::onFilterChanged);

    refresh();
}

void TestPointPanel::setProject(Project *project)
{
    m_project = project;
    refresh();
}

void TestPointPanel::setContextComponent(const QString &uid)
{
    m_contextComponent = uid;
}

const QVector<TestPoint> &TestPointPanel::points() const
{
    static const QVector<TestPoint> empty;
    return m_project != nullptr ? m_project->data().testPoints : empty;
}

const TestPoint *TestPointPanel::testPointByUid(const QString &uid) const
{
    for (const TestPoint &point : points()) {
        if (point.uid == uid)
            return &point;
    }
    return nullptr;
}

void TestPointPanel::refresh()
{
    m_tree->clear();

    static const QVector<Measurement> noMeasurements;
    const QVector<Measurement> &measurements =
        m_project != nullptr ? m_project->data().measurements : noMeasurements;
    for (const TestPoint &point : points()) {
        if (!passesFilter(point))
            continue;

        const Measurement latest = latestMeasurement(point.uid, measurements);
        QString component;
        const Board *board = m_project != nullptr ? m_project->primaryBoard() : nullptr;
        if (board != nullptr) {
            if (const Component *owner = board->componentByUid(point.componentUid))
                component = owner->reference;
        }

        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, point.identifier);
        item->setText(1, component);
        item->setText(2, point.netName);
        item->setText(3, point.hasExpectedValue
                             ? QStringLiteral("%1%2")
                                   .arg(formatValue(point.expectedValue, point.unit))
                                   .arg(point.hasTolerance
                                            ? QStringLiteral(" +/-%1%2")
                                                  .arg(trimNumber(point.tolerance))
                                                  .arg(point.toleranceIsPercent
                                                           ? QStringLiteral(" %")
                                                           : QString())
                                            : QString())
                             : tr("(not recorded)"));
        item->setText(4, latest.uid.isEmpty() ? tr("(never measured)")
                                              : formatValue(latest.value, latest.unit));
        item->setText(5, statusLabel(latest.status));
        item->setForeground(5, QColor(statusColor(latest.status)));
        item->setData(0, Qt::UserRole, point.uid);
        item->setToolTip(0, QStringList{ point.category, point.probeNotes, point.testConditions }
                                .join(QStringLiteral("  |  ")));
        m_tree->addTopLevelItem(item);
    }

    const bool hasSelection = !m_selected.isEmpty();
    m_measureButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
    m_status->setText(coverage().summary());

    if (hasSelection)
        showMeasurements(m_selected);
    else
        m_measurements->clear();
}

bool TestPointPanel::passesFilter(const TestPoint &point) const
{
    const QString mode = m_filter->currentData().toString();
    if (mode.isEmpty() || m_project == nullptr)
        return true;
    const Measurement latest = latestMeasurement(point.uid, m_project->data().measurements);
    switch (latest.status) {
    case MeasurementStatus::NotTested:
        return mode == QStringLiteral("untested");
    case MeasurementStatus::Pass:
        return mode == QStringLiteral("passing");
    case MeasurementStatus::Fail:
        return mode == QStringLiteral("failing");
    case MeasurementStatus::Unknown:
        return mode == QStringLiteral("unknown");
    }
    return true;
}

void TestPointPanel::showMeasurements(const QString &testPointUid)
{
    m_measurements->clear();
    if (m_project == nullptr)
        return;

    // Newest first: the list is walked backwards, and the sort below is a
    // stable one, so readings taken in the same millisecond keep the order in
    // which they were recorded.
    QVector<Measurement> mine;
    const QVector<Measurement> &all = m_project->data().measurements;
    for (auto it = all.crbegin(); it != all.crend(); ++it) {
        if (it->testPointUid == testPointUid)
            mine.append(*it);
    }
    std::stable_sort(mine.begin(), mine.end(), [](const Measurement &a, const Measurement &b) {
        return a.timestamp > b.timestamp;
    });
    for (const Measurement &measurement : mine) {
        auto *item = new QTreeWidgetItem(m_measurements);
        item->setText(0, measurement.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm")));
        item->setText(1, formatValue(measurement.value, measurement.unit));
        item->setText(2, statusLabel(measurement.status));
        item->setForeground(2, QColor(statusColor(measurement.status)));
        item->setText(3, measurementExplanation(measurement));
        item->setToolTip(1, QStringList{ measurement.instrument, measurement.conditions,
                                        measurement.notes }
                                .join(QStringLiteral("  |  ")));
        m_measurements->addTopLevelItem(item);
    }
}

int TestPointPanel::visibleCount() const { return m_tree->topLevelItemCount(); }

QString TestPointPanel::selectedTestPointUid() const { return m_selected; }

void TestPointPanel::selectTestPoint(const QString &uid)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        if (m_tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == uid) {
            m_tree->setCurrentItem(m_tree->topLevelItem(i));
            return;
        }
    }
}

TestCoverage TestPointPanel::coverage() const
{
    if (m_project == nullptr)
        return TestCoverage();
    return coverageOf(m_project->data().testPoints, m_project->data().measurements);
}

QString TestPointPanel::addTestPoint(const TestPoint &point, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("There is no open project to add a test point to.");
        return QString();
    }
    if (point.identifier.trimmed().isEmpty()) {
        if (error)
            *error = tr("A test point needs an identifier.");
        return QString();
    }

    TestPoint stored = point;
    if (stored.uid.isEmpty())
        stored.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    stored.identifier = stored.identifier.trimmed();

    m_project->data().testPoints.append(stored);
    m_selected = stored.uid;
    refresh();
    logInfo(QStringLiteral("testpoints"),
            tr("test point %1 added (%2)").arg(stored.identifier, stored.category));
    emit testPointsChanged();
    return stored.uid;
}

QString TestPointPanel::recordMeasurement(const QString &testPointUid, double value,
                                          const QString &unit, const QString &instrument,
                                          const QString &conditions, const QString &notes,
                                          QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("There is no open project to record a measurement in.");
        return QString();
    }
    const TestPoint *point = testPointByUid(testPointUid);
    if (point == nullptr) {
        if (error)
            *error = tr("There is no test point with that id.");
        return QString();
    }
    if (std::isnan(value) || std::isinf(value)) {
        if (error)
            *error = tr("The measured value is not a number.");
        return QString();
    }

    const Measurement measurement = eda::recordMeasurement(*point, value, unit.trimmed(),
                                                           QDateTime::currentDateTimeUtc(),
                                                           instrument, conditions, notes);
    m_project->data().measurements.append(measurement);
    refresh();
    logInfo(QStringLiteral("testpoints"),
            tr("%1: %2 -- %3")
                .arg(point->identifier, formatValue(measurement.value, measurement.unit),
                     measurementExplanation(measurement)));
    emit testPointsChanged();
    return measurement.uid;
}

bool TestPointPanel::removeTestPoint(const QString &uid, QString *error)
{
    if (m_project == nullptr) {
        if (error)
            *error = tr("There is no open project.");
        return false;
    }
    int index = -1;
    for (int i = 0; i < m_project->data().testPoints.size(); ++i) {
        if (m_project->data().testPoints.at(i).uid == uid) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        if (error)
            *error = tr("There is no test point with that id.");
        return false;
    }

    const QString identifier = m_project->data().testPoints.at(index).identifier;
    m_project->data().testPoints.removeAt(index);
    // The readings stay: a measurement is a fact about the board, and dropping
    // it because the test point was removed would rewrite the history.
    if (m_selected == uid)
        m_selected.clear();
    refresh();
    logInfo(QStringLiteral("testpoints"),
            tr("test point %1 removed; its measurements were kept").arg(identifier));
    emit testPointsChanged();
    return true;
}

void TestPointPanel::onAdd()
{
    if (m_project == nullptr) {
        QMessageBox::information(this, tr("Test points"),
                                 tr("Open or create a project first: test points belong to a "
                                    "board and are stored with the project."));
        return;
    }
    const Board *board = m_project->primaryBoard();
    if (board == nullptr) {
        QMessageBox::information(this, tr("Test points"),
                                 tr("Import a board file first: a test point is a place on a "
                                    "board."));
        return;
    }

    const QString identifier = suggestTestPointIdentifier(*board, m_contextComponent, QString(),
                                                          m_project->data().testPoints);
    TestPointDialog dialog(board, m_contextComponent, identifier, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    TestPoint point = dialog.testPoint(QString());
    if (point.identifier.isEmpty())
        point.identifier = identifier;

    // The position of the probe comes from the pad, when one was chosen.
    const Component *component = board->componentByUid(point.componentUid);
    if (component != nullptr && !point.pinOrPad.isEmpty()) {
        for (const Pad &pad : component->pads) {
            const QString label = pad.pin.isEmpty() ? pad.id : pad.pin;
            if (label != point.pinOrPad)
                continue;
            point.position = { component->position.x + pad.position.x,
                               component->position.y + pad.position.y };
            point.hasPosition = true;
            if (point.netName.isEmpty())
                point.netName = pad.netName;
            break;
        }
    }

    QString error;
    if (addTestPoint(point, &error).isEmpty())
        QMessageBox::critical(this, tr("Test points"), error);
}

void TestPointPanel::onMeasure()
{
    const TestPoint *point = testPointByUid(m_selected);
    if (point == nullptr)
        return;

    MeasurementDialog dialog(*point, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString error;
    if (recordMeasurement(m_selected, dialog.value(), dialog.unit(), dialog.instrument(),
                          dialog.conditions(), dialog.notes(), &error)
            .isEmpty()) {
        QMessageBox::critical(this, tr("Measurement"), error);
    }
}

void TestPointPanel::onRemove()
{
    if (m_selected.isEmpty())
        return;
    const TestPoint *point = testPointByUid(m_selected);
    if (point == nullptr)
        return;
    if (QMessageBox::question(this, tr("Remove test point"),
                              tr("Remove %1? Its measurements are kept.")
                                  .arg(point->identifier),
                              QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!removeTestPoint(m_selected, &error))
        QMessageBox::critical(this, tr("Remove test point"), error);
}

void TestPointPanel::onSelectionChanged()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    const QString uid = item != nullptr ? item->data(0, Qt::UserRole).toString() : QString();
    if (uid == m_selected)
        return;
    m_selected = uid;
    showMeasurements(uid);
    m_measureButton->setEnabled(!uid.isEmpty());
    m_removeButton->setEnabled(!uid.isEmpty());
    emit testPointSelected(uid);
}

void TestPointPanel::onFilterChanged() { refresh(); }

} // namespace eda
