#include "ComponentDetails.h"

#include "../core/Logging.h"
#include "../core/Project.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace eda {
namespace {

// Dialog for one piece of knowledge. It asks for the source as loudly as for
// the statement, because a fact whose origin is unknown is worth much less.
class FactDialog : public QDialog
{
public:
    explicit FactDialog(const QString &componentName, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(componentName.isEmpty() ? tr("Add knowledge")
                                              : tr("Add knowledge about %1").arg(componentName));
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        m_topic = new QComboBox(this);
        m_topic->setEditable(true);
        m_topic->addItems(knowledgeTopics());
        form->addRow(tr("Topic:"), m_topic);

        m_statement = new QPlainTextEdit(this);
        m_statement->setPlaceholderText(
            tr("What is known? For example: \"Pin 1 is the cathode, marked with a band.\""));
        m_statement->setFixedHeight(70);
        form->addRow(tr("Statement:"), m_statement);

        auto *valueRow = new QHBoxLayout();
        m_value = new QLineEdit(this);
        m_value->setPlaceholderText(tr("optional"));
        valueRow->addWidget(m_value, 2);
        m_unit = new QLineEdit(this);
        m_unit->setPlaceholderText(tr("unit"));
        m_unit->setFixedWidth(80);
        valueRow->addWidget(m_unit, 1);
        form->addRow(tr("Value:"), valueRow);

        m_conditions = new QLineEdit(this);
        m_conditions->setPlaceholderText(tr("optional: at 25 C, board unpowered, ..."));
        form->addRow(tr("Conditions:"), m_conditions);

        form->addRow(new QLabel(tr("Where does it come from?"), this));

        m_sourceTitle = new QLineEdit(this);
        m_sourceTitle->setPlaceholderText(tr("datasheet, application note, your own bench"));
        form->addRow(tr("Source:"), m_sourceTitle);

        m_sourcePublisher = new QLineEdit(this);
        form->addRow(tr("Publisher:"), m_sourcePublisher);

        m_sourceUrl = new QLineEdit(this);
        m_sourceUrl->setPlaceholderText(tr("https://"));
        form->addRow(tr("URL:"), m_sourceUrl);

        m_sourcePath = new QLineEdit(this);
        m_sourcePath->setPlaceholderText(tr("or a file on this computer"));
        form->addRow(tr("File:"), m_sourcePath);

        m_location = new QLineEdit(this);
        m_location->setPlaceholderText(tr("page 12, section 6.3"));
        form->addRow(tr("Location:"), m_location);

        m_status = new QComboBox(this);
        const VerificationStatus offered[] = {
            VerificationStatus::OfficialSourceVerified, VerificationStatus::UserVerified,
            VerificationStatus::ExperimentallyVerified, VerificationStatus::Unverified,
            VerificationStatus::NeedsReview, VerificationStatus::Inferred,
            VerificationStatus::AiSuggested
        };
        for (VerificationStatus status : offered)
            m_status->addItem(eda::toString(status), eda::toString(status));
        form->addRow(tr("Trust:"), m_status);

        layout->addLayout(form);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &FactDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &FactDialog::reject);
        layout->addWidget(buttons);

        auto *ok = buttons->button(QDialogButtonBox::Ok);
        connect(m_statement, &QPlainTextEdit::textChanged, this, [this, ok]() {
            ok->setEnabled(!m_statement->toPlainText().trimmed().isEmpty());
        });
        ok->setEnabled(false);
    }

    Fact fact(const QString &componentUid) const
    {
        Fact fact;
        fact.componentUid = componentUid;
        fact.topic = m_topic->currentText().trimmed();
        fact.statement = m_statement->toPlainText().trimmed();
        fact.value = m_value->text().trimmed();
        fact.unit = m_unit->text().trimmed();
        fact.conditions = m_conditions->text().trimmed();

        fact.provenance.title = m_sourceTitle->text().trimmed();
        fact.provenance.publisher = m_sourcePublisher->text().trimmed();
        fact.provenance.url = m_sourceUrl->text().trimmed();
        fact.provenance.localPath = m_sourcePath->text().trimmed();
        fact.provenance.location = m_location->text().trimmed();
        fact.provenance.retrieved = QDateTime::currentDateTimeUtc();

        const bool sourced = !fact.provenance.title.isEmpty() || !fact.provenance.url.isEmpty()
                             || !fact.provenance.localPath.isEmpty();
        fact.provenance.status = sourced
                                     ? verificationStatusFromString(m_status->currentData().toString())
                                     : VerificationStatus::Unverified;
        fact.provenance.valueSource = sourced ? ValueSource::ExtractedFromSource
                                              : ValueSource::ManualEntry;
        if (!sourced)
            fact.provenance.notes = QStringLiteral("no source was given");
        return fact;
    }

private:
    QComboBox *m_topic = nullptr;
    QPlainTextEdit *m_statement = nullptr;
    QLineEdit *m_value = nullptr;
    QLineEdit *m_unit = nullptr;
    QLineEdit *m_conditions = nullptr;
    QLineEdit *m_sourceTitle = nullptr;
    QLineEdit *m_sourcePublisher = nullptr;
    QLineEdit *m_sourceUrl = nullptr;
    QLineEdit *m_sourcePath = nullptr;
    QLineEdit *m_location = nullptr;
    QComboBox *m_status = nullptr;
};

QString statusText(const Provenance &provenance)
{
    return eda::toString(provenance.status).replace(QLatin1Char('-'), QLatin1Char(' '));
}

} // namespace

ComponentDetails::ComponentDetails(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_title = new QLabel(tr("No component selected"), this);
    QFont titleFont = m_title->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    m_title->setFont(titleFont);
    layout->addWidget(m_title);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    layout->addWidget(m_summary);

    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);

    // --- the component itself --------------------------------------------
    auto *componentPage = new QWidget(tabs);
    auto *componentLayout = new QVBoxLayout(componentPage);
    auto *form = new QFormLayout();

    // Object names are what the automated tests (and a debugging session) find
    // the fields by.
    m_value = new QLineEdit(componentPage);
    m_value->setObjectName(QStringLiteral("details.value"));
    m_description = new QLineEdit(componentPage);
    m_description->setObjectName(QStringLiteral("details.description"));
    m_category = new QComboBox(componentPage);
    m_category->setObjectName(QStringLiteral("details.category"));
    m_manufacturer = new QLineEdit(componentPage);
    m_manufacturer->setObjectName(QStringLiteral("details.manufacturer"));
    m_mpn = new QLineEdit(componentPage);
    m_mpn->setObjectName(QStringLiteral("details.mpn"));
    m_package = new QLineEdit(componentPage);
    m_package->setObjectName(QStringLiteral("details.package"));
    m_datasheetUrl = new QLineEdit(componentPage);
    m_datasheetUrl->setObjectName(QStringLiteral("details.datasheetUrl"));
    m_datasheetPath = new QLineEdit(componentPage);
    m_datasheetPath->setObjectName(QStringLiteral("details.datasheetPath"));
    m_notes = new QPlainTextEdit(componentPage);
    m_notes->setObjectName(QStringLiteral("details.notes"));
    m_notes->setFixedHeight(56);

    m_category->addItem(tr("Unknown"), eda::toString(ComponentCategory::Unknown));
    m_category->addItem(tr("Resistor"), eda::toString(ComponentCategory::Resistor));
    m_category->addItem(tr("Capacitor"), eda::toString(ComponentCategory::Capacitor));
    m_category->addItem(tr("Inductor"), eda::toString(ComponentCategory::Inductor));
    m_category->addItem(tr("Diode"), eda::toString(ComponentCategory::Diode));
    m_category->addItem(tr("LED"), eda::toString(ComponentCategory::Led));
    m_category->addItem(tr("Transistor"), eda::toString(ComponentCategory::Transistor));
    m_category->addItem(tr("Integrated circuit"),
                        eda::toString(ComponentCategory::IntegratedCircuit));
    m_category->addItem(tr("Connector"), eda::toString(ComponentCategory::Connector));
    m_category->addItem(tr("Crystal"), eda::toString(ComponentCategory::Crystal));
    m_category->addItem(tr("Fuse"), eda::toString(ComponentCategory::Fuse));
    m_category->addItem(tr("Relay"), eda::toString(ComponentCategory::Relay));
    m_category->addItem(tr("Transformer"), eda::toString(ComponentCategory::Transformer));
    m_category->addItem(tr("Switch"), eda::toString(ComponentCategory::Switch));
    m_category->addItem(tr("Test point"), eda::toString(ComponentCategory::TestPoint));
    m_category->addItem(tr("Mechanical"), eda::toString(ComponentCategory::Mechanical));
    m_category->addItem(tr("Other"), eda::toString(ComponentCategory::Other));

    form->addRow(tr("Value:"), m_value);
    form->addRow(tr("Description:"), m_description);
    form->addRow(tr("Category:"), m_category);
    form->addRow(tr("Manufacturer:"), m_manufacturer);
    form->addRow(tr("Part number:"), m_mpn);
    form->addRow(tr("Package:"), m_package);
    form->addRow(tr("Datasheet URL:"), m_datasheetUrl);
    form->addRow(tr("Datasheet file:"), m_datasheetPath);
    form->addRow(tr("Notes:"), m_notes);
    componentLayout->addLayout(form);

    m_provenance = new QLabel(componentPage);
    m_provenance->setWordWrap(true);
    m_provenance->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    componentLayout->addWidget(m_provenance);

    auto *padsGroup = new QGroupBox(tr("Pads"), componentPage);
    auto *padsLayout = new QVBoxLayout(padsGroup);
    m_pads = new QTreeWidget(padsGroup);
    m_pads->setObjectName(QStringLiteral("details.pads"));
    m_pads->setColumnCount(6);
    m_pads->setHeaderLabels({ tr("Pin"), tr("Name"), tr("Net"), tr("X"), tr("Y"), tr("Size") });
    m_pads->setRootIsDecorated(false);
    m_pads->setAlternatingRowColors(true);
    m_pads->setUniformRowHeights(true);
    m_pads->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_pads->setFixedHeight(120);
    padsLayout->addWidget(m_pads);
    componentLayout->addWidget(padsGroup);
    componentLayout->addStretch(1);
    tabs->addTab(componentPage, tr("Component"));

    // --- what is known about it ------------------------------------------
    auto *knowledgePage = new QWidget(tabs);
    auto *knowledgeLayout = new QVBoxLayout(knowledgePage);

    m_search = new QLineEdit(knowledgePage);
    m_search->setObjectName(QStringLiteral("details.search"));
    m_search->setPlaceholderText(tr("Search every fact, or leave empty for this component..."));
    m_search->setClearButtonEnabled(true);
    knowledgeLayout->addWidget(m_search);

    m_facts = new QListWidget(knowledgePage);
    m_facts->setObjectName(QStringLiteral("details.facts"));
    m_facts->setAlternatingRowColors(true);
    knowledgeLayout->addWidget(m_facts, 1);

    auto *buttons = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add..."), knowledgePage);
    auto *removeButton = new QPushButton(tr("Remove"), knowledgePage);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch(1);
    knowledgeLayout->addLayout(buttons);

    m_factStatus = new QLabel(knowledgePage);
    m_factStatus->setWordWrap(true);
    m_factStatus->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    knowledgeLayout->addWidget(m_factStatus);
    tabs->addTab(knowledgePage, tr("Knowledge"));

    for (QLineEdit *field : { m_value, m_description, m_manufacturer, m_mpn, m_package,
                              m_datasheetUrl, m_datasheetPath }) {
        connect(field, &QLineEdit::editingFinished, this, &ComponentDetails::onFieldEdited);
    }
    connect(m_category, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ComponentDetails::onFieldEdited);
    connect(m_notes, &QPlainTextEdit::textChanged, this, &ComponentDetails::onFieldEdited);
    connect(m_search, &QLineEdit::textChanged, this, &ComponentDetails::onSearchChanged);
    connect(addButton, &QPushButton::clicked, this, &ComponentDetails::onAddFact);
    connect(removeButton, &QPushButton::clicked, this, &ComponentDetails::onRemoveFact);
    connect(m_facts, &QListWidget::itemActivated, this, &ComponentDetails::onFactActivated);

    refresh();
}

void ComponentDetails::setProject(Project *project)
{
    m_project = project;
    refresh();
}

void ComponentDetails::setDatabase(Database *database)
{
    m_database = database;
    refresh();
}

const Component *ComponentDetails::component() const
{
    if (m_project == nullptr || m_uid.isEmpty())
        return nullptr;
    const Board *board = m_project->primaryBoard();
    return board != nullptr ? board->componentByUid(m_uid) : nullptr;
}

Component *ComponentDetails::editableComponent()
{
    if (m_project == nullptr || m_uid.isEmpty())
        return nullptr;
    Board *board = m_project->primaryBoard();
    if (board == nullptr)
        return nullptr;
    const int index = board->indexOfComponent(m_uid);
    return index < 0 ? nullptr : &board->components[index];
}

void ComponentDetails::selectComponent(const QString &uid)
{
    if (m_uid == uid)
        return;
    m_uid = uid;
    refresh();
}

void ComponentDetails::refresh()
{
    const Component *component = this->component();
    m_updating = true;

    if (component == nullptr) {
        m_title->setText(tr("No component selected"));
        m_summary->setText(m_project != nullptr && m_project->primaryBoard() != nullptr
                               ? tr("Pick a component in the list or click it on the board.")
                               : tr("Import a board file to see its components."));
        m_value->clear();
        m_description->clear();
        m_category->setCurrentIndex(0);
        m_manufacturer->clear();
        m_mpn->clear();
        m_package->clear();
        m_datasheetUrl->clear();
        m_datasheetPath->clear();
        m_notes->clear();
        m_provenance->clear();
        m_pads->clear();
    } else {
        const QString name = component->reference.isEmpty() ? component->uid : component->reference;
        m_title->setText(component->value.isEmpty() ? name
                                                   : QStringLiteral("%1  %2").arg(name,
                                                                                  component->value));
        QStringList summary;
        if (!component->footprint.isEmpty())
            summary.append(component->footprint);
        summary.append(eda::toString(component->side));
        summary.append(tr("%n pad(s)", "", component->pads.size()));
        if (component->hasGeometry)
            summary.append(tr("at X %1 mm, Y %2 mm")
                               .arg(component->position.x, 0, 'f', 2)
                               .arg(component->position.y, 0, 'f', 2));
        else
            summary.append(tr("no position in the imported data"));
        if (!component->nets.isEmpty())
            summary.append(tr("nets: %1").arg(component->nets.join(QStringLiteral(", "))));
        m_summary->setText(summary.join(QStringLiteral("  |  ")));

        m_value->setText(component->value);
        m_description->setText(component->description);
        m_category->setCurrentIndex(
            m_category->findData(eda::toString(component->category)));
        m_manufacturer->setText(component->manufacturer);
        m_mpn->setText(component->manufacturerPartNumber);
        m_package->setText(component->package);
        m_datasheetUrl->setText(component->datasheetUrl);
        m_datasheetPath->setText(component->datasheetPath);
        m_notes->setPlainText(component->notes);

        QStringList origin;
        if (!component->provenance.localPath.isEmpty())
            origin.append(QFileInfo(component->provenance.localPath).fileName());
        if (!component->provenance.url.isEmpty())
            origin.append(component->provenance.url);
        if (!component->provenance.location.isEmpty())
            origin.append(component->provenance.location);
        m_provenance->setText(
            tr("Source: %1  |  %2")
                .arg(origin.isEmpty() ? tr("unknown") : origin.join(QStringLiteral("  |  ")),
                     statusText(component->provenance)));

        m_pads->clear();
        for (const Pad &pad : component->pads) {
            auto *item = new QTreeWidgetItem(m_pads);
            item->setText(0, pad.pin.isEmpty() ? pad.id : pad.pin);
            item->setText(1, pad.pinName);
            item->setText(2, pad.netName.isEmpty() ? tr("(not connected)") : pad.netName);
            item->setText(3, QString::number(pad.position.x, 'f', 2));
            item->setText(4, QString::number(pad.position.y, 'f', 2));
            item->setText(5, QStringLiteral("%1 x %2")
                                 .arg(pad.size.width, 0, 'f', 2)
                                 .arg(pad.size.height, 0, 'f', 2));
            item->setToolTip(0, tr("%1 pad on the %2 side")
                                    .arg(eda::toString(pad.shape), eda::toString(pad.side)));
            m_pads->addTopLevelItem(item);
        }
    }

    m_updating = false;
    showFacts();
}

void ComponentDetails::showFacts()
{
    m_facts->clear();
    m_factIds.clear();

    const bool available = m_database != nullptr && m_database->isOpen();
    m_facts->setEnabled(available);

    if (!available) {
        const QString reason = m_database != nullptr && !m_database->lastError().isEmpty()
                                   ? m_database->lastError()
                                   : tr("the knowledge database is not open");
        m_factStatus->setText(tr("Knowledge is not available: %1").arg(reason));
        return;
    }

    const QString query = m_search->text().trimmed();
    QVector<Fact> facts;
    if (!query.isEmpty())
        facts = m_database->searchFacts(query);
    else if (m_uid.isEmpty())
        facts = m_database->recentFacts();           // nothing selected: everything
    else
        facts = m_database->factsForComponent(m_uid);
    for (const Fact &fact : facts) {
        auto *item = new QListWidgetItem(fact.summary(), m_facts);
        QStringList tooltip;
        if (!fact.provenance.title.isEmpty())
            tooltip.append(fact.provenance.title);
        if (!fact.provenance.location.isEmpty())
            tooltip.append(fact.provenance.location);
        tooltip.append(statusText(fact.provenance));
        item->setToolTip(tooltip.join(QStringLiteral("  |  ")));
        m_factIds.insert(m_facts->count() - 1, fact.id);
    }

    if (query.isEmpty()) {
        if (m_uid.isEmpty())
            m_factStatus->setText(tr("%n fact(s) stored, newest first.", "", facts.size()));
        else
            m_factStatus->setText(tr("%n fact(s) about this component.", "", facts.size()));
    } else {
        const QString mode = m_database->lastSearchMode() == QStringLiteral("fts5")
                                 ? tr("full text index")
                                 : tr("text scan");
        m_factStatus->setText(tr("%n match(es) for \"%2\" (%3).", "", facts.size())
                                  .arg(query, mode));
    }
}

int ComponentDetails::factCount() const { return m_facts->count(); }

QString ComponentDetails::summaryText() const
{
    return m_title->text() + QStringLiteral("  |  ") + m_summary->text();
}

void ComponentDetails::onFieldEdited()
{
    if (m_updating)
        return;
    Component *component = editableComponent();
    if (component == nullptr)
        return;

    auto apply = [](const QString &value, QString &target, bool &changed) {
        if (target == value)
            return;
        target = value;
        changed = true;
    };

    bool changed = false;
    apply(m_value->text().trimmed(), component->value, changed);
    apply(m_description->text().trimmed(), component->description, changed);
    apply(m_manufacturer->text().trimmed(), component->manufacturer, changed);
    apply(m_mpn->text().trimmed(), component->manufacturerPartNumber, changed);
    apply(m_package->text().trimmed(), component->package, changed);
    apply(m_datasheetUrl->text().trimmed(), component->datasheetUrl, changed);
    apply(m_datasheetPath->text().trimmed(), component->datasheetPath, changed);
    apply(m_notes->toPlainText(), component->notes, changed);

    const ComponentCategory category =
        componentCategoryFromString(m_category->currentData().toString());
    if (category != component->category) {
        component->category = category;
        changed = true;
    }

    if (!changed)
        return;

    // The component is now part file data and part hand entered data: the
    // origin of the file data is kept, and the record says so.
    component->provenance.status = VerificationStatus::NeedsReview;
    component->provenance.valueSource = ValueSource::ManualEntry;
    component->provenance.notes = QStringLiteral("completed by hand in the workbench");

    if (m_database != nullptr && m_database->isOpen()
        && !component->manufacturerPartNumber.isEmpty()) {
        ComponentRecord record;
        record.mpn = component->manufacturerPartNumber;
        record.manufacturer = component->manufacturer;
        record.category = component->category;
        record.description = component->description;
        record.package = component->package;
        record.datasheetUrl = component->datasheetUrl;
        record.datasheetPath = component->datasheetPath;
        record.provenance.status = VerificationStatus::NeedsReview;
        record.provenance.valueSource = ValueSource::ManualEntry;
        record.provenance.retrieved = QDateTime::currentDateTimeUtc();
        QString error;
        if (!m_database->upsertComponent(&record, &error))
            logError(QStringLiteral("database"), error);
    }

    emit componentEdited(component->uid);
    refresh();
}

void ComponentDetails::setSearchText(const QString &text)
{
    if (m_search->text() == text)
        return;
    m_search->setText(text);
    showFacts();
}

void ComponentDetails::onSearchChanged() { showFacts(); }

void ComponentDetails::onAddFact()
{
    if (m_database == nullptr || !m_database->isOpen())
        return;
    const Component *component = this->component();
    FactDialog dialog(component != nullptr ? component->reference : QString(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Fact fact = dialog.fact(m_uid);
    QString error;
    if (!m_database->addFact(&fact, &error)) {
        QMessageBox::critical(this, tr("Add knowledge"), error);
        return;
    }
    logInfo(QStringLiteral("database"),
            QStringLiteral("stored a fact (%1) %2").arg(fact.topic).arg(fact.id));
    showFacts();
}

void ComponentDetails::onRemoveFact()
{
    if (m_database == nullptr || !m_database->isOpen())
        return;
    const int row = m_facts->currentRow();
    if (row < 0 || !m_factIds.contains(row))
        return;
    const qlonglong id = m_factIds.value(row);
    if (QMessageBox::question(this, tr("Remove knowledge"),
                              tr("Remove this fact from the knowledge database?"),
                              QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_database->removeFact(id, &error))
        QMessageBox::critical(this, tr("Remove knowledge"), error);
    showFacts();
}

void ComponentDetails::onFactActivated(QListWidgetItem *item)
{
    if (item == nullptr || m_database == nullptr || !m_database->isOpen())
        return;
    const int row = m_facts->row(item);
    const QVector<Fact> facts = m_search->text().trimmed().isEmpty()
                                    ? m_database->factsForComponent(m_uid)
                                    : m_database->searchFacts(m_search->text().trimmed());
    for (const Fact &fact : facts) {
        if (fact.id != m_factIds.value(row))
            continue;
        QStringList lines;
        lines.append(fact.statement);
        if (!fact.value.isEmpty())
            lines.append(tr("Value: %1 %2").arg(fact.value, fact.unit).trimmed());
        if (!fact.conditions.isEmpty())
            lines.append(tr("Conditions: %1").arg(fact.conditions));
        if (!fact.provenance.title.isEmpty())
            lines.append(tr("Source: %1").arg(fact.provenance.title));
        if (!fact.provenance.publisher.isEmpty())
            lines.append(tr("Publisher: %1").arg(fact.provenance.publisher));
        if (!fact.provenance.location.isEmpty())
            lines.append(tr("Location: %1").arg(fact.provenance.location));
        if (!fact.provenance.url.isEmpty())
            lines.append(fact.provenance.url);
        if (!fact.provenance.localPath.isEmpty())
            lines.append(fact.provenance.localPath);
        lines.append(tr("Trust: %1").arg(statusText(fact.provenance)));
        if (!fact.provenance.notes.isEmpty())
            lines.append(fact.provenance.notes);
        QMessageBox::information(this, tr("Knowledge"), lines.join(QStringLiteral("\n")));
        return;
    }
}

} // namespace eda
