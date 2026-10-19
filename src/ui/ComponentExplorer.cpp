#include "ComponentExplorer.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace eda {

ComponentExplorer::ComponentExplorer(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search reference, value or part number..."));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &ComponentExplorer::applyFilters);
    layout->addWidget(m_search);

    auto *filterRow = new QHBoxLayout();
    m_categoryFilter = new QComboBox(this);
    m_categoryFilter->addItem(tr("All categories"), QString());
    m_categoryFilter->addItem(tr("Integrated circuit"), QStringLiteral("ic"));
    m_categoryFilter->addItem(tr("Resistor"), QStringLiteral("resistor"));
    m_categoryFilter->addItem(tr("Capacitor"), QStringLiteral("capacitor"));
    m_categoryFilter->addItem(tr("Inductor"), QStringLiteral("inductor"));
    m_categoryFilter->addItem(tr("Diode"), QStringLiteral("diode"));
    m_categoryFilter->addItem(tr("Transistor"), QStringLiteral("transistor"));
    m_categoryFilter->addItem(tr("Connector"), QStringLiteral("connector"));
    m_categoryFilter->addItem(tr("Other"), QStringLiteral("other"));
    connect(m_categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ComponentExplorer::applyFilters);
    filterRow->addWidget(m_categoryFilter, 1);

    m_sideFilter = new QComboBox(this);
    m_sideFilter->addItem(tr("Both sides"), QString());
    m_sideFilter->addItem(tr("Top"), QStringLiteral("top"));
    m_sideFilter->addItem(tr("Bottom"), QStringLiteral("bottom"));
    connect(m_sideFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ComponentExplorer::applyFilters);
    filterRow->addWidget(m_sideFilter);
    layout->addLayout(filterRow);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({ tr("Reference"), tr("Value"), tr("Package"), tr("Side") });
    m_tree->setRootIsDecorated(false);
    m_tree->setSortingEnabled(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            &ComponentExplorer::onItemSelectionChanged);
    connect(m_tree, &QTreeWidget::itemActivated, this, &ComponentExplorer::onItemActivated);
    layout->addWidget(m_tree, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #94A3B8;"));
    layout->addWidget(m_statusLabel);

    rebuild();
}

void ComponentExplorer::setBoard(const Board *board)
{
    m_board = board;
    rebuild();
}

void ComponentExplorer::rebuild()
{
    m_tree->clear();
    m_itemsByUid.clear();

    if (!m_board) {
        m_statusLabel->setText(tr("No board loaded."));
        return;
    }

    for (const Component &component : m_board->components) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, component.reference.isEmpty() ? tr("(no reference)") : component.reference);
        item->setText(1, component.value);
        item->setText(2, component.package.isEmpty() ? component.footprint : component.package);
        item->setText(3, eda::toString(component.side));
        item->setData(0, Qt::UserRole, component.uid);
        if (!component.hasGeometry)
            item->setToolTip(0, tr("This component has no geometry in the imported data and "
                                   "cannot be located on the board."));
        m_tree->addTopLevelItem(item);
        m_itemsByUid.insert(component.uid, item);
    }

    applyFilters();
}

bool ComponentExplorer::matchesFilter(const Component &component) const
{
    const QString needle = m_search->text().trimmed();
    if (!needle.isEmpty()) {
        const bool hit = component.reference.contains(needle, Qt::CaseInsensitive)
            || component.value.contains(needle, Qt::CaseInsensitive)
            || component.manufacturerPartNumber.contains(needle, Qt::CaseInsensitive)
            || component.manufacturer.contains(needle, Qt::CaseInsensitive)
            || component.footprint.contains(needle, Qt::CaseInsensitive);
        if (!hit)
            return false;
    }

    const QString category = m_categoryFilter->currentData().toString();
    if (!category.isEmpty() && eda::toString(component.category) != category)
        return false;

    const QString side = m_sideFilter->currentData().toString();
    if (!side.isEmpty() && eda::toString(component.side) != side)
        return false;

    return true;
}

void ComponentExplorer::applyFilters()
{
    if (!m_board)
        return;

    int visible = 0;
    for (const Component &component : m_board->components) {
        QTreeWidgetItem *item = m_itemsByUid.value(component.uid);
        if (!item)
            continue;
        const bool visibleRow = matchesFilter(component);
        item->setHidden(!visibleRow);
        if (visibleRow)
            ++visible;
    }

    const int total = m_board->components.size();
    if (visible == total)
        m_statusLabel->setText(tr("%n component(s)", "", total));
    else
        m_statusLabel->setText(tr("%1 of %n component(s) shown", "", total).arg(visible));
}

QString ComponentExplorer::selectedComponentUid() const
{
    const QList<QTreeWidgetItem *> selected = m_tree->selectedItems();
    if (selected.isEmpty())
        return QString();
    return selected.first()->data(0, Qt::UserRole).toString();
}

void ComponentExplorer::selectComponent(const QString &uid)
{
    if (uid.isEmpty()) {
        clearSelection();
        return;
    }
    QTreeWidgetItem *item = m_itemsByUid.value(uid);
    if (!item)
        return;
    m_tree->setCurrentItem(item);
    m_tree->scrollToItem(item);
}

void ComponentExplorer::clearSelection()
{
    m_tree->clearSelection();
    m_tree->setCurrentItem(nullptr);
}

int ComponentExplorer::visibleCount() const
{
    int count = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (!m_tree->topLevelItem(i)->isHidden())
            ++count;
    return count;
}

void ComponentExplorer::setFilterText(const QString &text)
{
    m_search->setText(text);
}

void ComponentExplorer::onItemSelectionChanged()
{
    // Announced for a programmatic selection as well: the board view follows the
    // list through this signal.  It cannot feed back, because
    // BoardView::setSelectedComponent() highlights an item without emitting
    // anything, and the only other source is a real mouse click.
    emit componentSelected(selectedComponentUid());
}

void ComponentExplorer::onItemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (!item)
        return;
    emit componentActivated(item->data(0, Qt::UserRole).toString());
}

} // namespace eda
