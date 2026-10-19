// Left dock: searchable component list.
//
// The list works on the project model and identifies a row by the component's
// stable uid, never by its position in the list, so filtering and sorting can
// never mix up two components. Selection is synchronised with the board view in
// both directions.
#pragma once

#include <QWidget>

#include "../core/ProjectModel.h"

#include <QHash>

class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace eda {

class ComponentExplorer : public QWidget
{
    Q_OBJECT
public:
    explicit ComponentExplorer(QWidget *parent = nullptr);

    void setBoard(const Board *board);
    const Board *board() const { return m_board; }

    QString selectedComponentUid() const;
    void selectComponent(const QString &uid);

    int visibleCount() const;

public slots:
    void clearSelection();
    void setFilterText(const QString &text);

signals:
    void componentSelected(const QString &uid);
    void componentActivated(const QString &uid);

private slots:
    void onItemSelectionChanged();
    void onItemActivated(QTreeWidgetItem *item, int column);
    void applyFilters();

private:
    void rebuild();
    bool matchesFilter(const Component &component) const;

    const Board *m_board = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_categoryFilter = nullptr;
    QComboBox *m_sideFilter = nullptr;
    QLabel *m_statusLabel = nullptr;
    QHash<QString, QTreeWidgetItem *> m_itemsByUid;
};

} // namespace eda
