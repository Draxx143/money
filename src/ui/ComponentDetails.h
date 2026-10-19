// Right hand panel: everything about one component.
//
// The panel shows what the imported file said about the selected component,
// lets the user complete it (a part number, a datasheet, a note) and lists the
// knowledge that is stored about it in the local database. Every field the user
// fills in is marked as entered by hand, so a value taken from a board file can
// never be confused with one somebody looked up and confirmed.
#pragma once

#include <QWidget>

#include "../core/Database.h"
#include "../core/ProjectModel.h"

#include <QHash>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QTreeWidget;

namespace eda {

class Project;

class ComponentDetails : public QWidget
{
    Q_OBJECT
public:
    explicit ComponentDetails(QWidget *parent = nullptr);

    void setProject(Project *project);
    void setDatabase(Database *database);

    QString componentUid() const { return m_uid; }
    // What the panel is showing, for the status bar and for tests.
    QString summaryText() const;
    int factCount() const;

public slots:
    void selectComponent(const QString &uid);
    void refresh();
    void setSearchText(const QString &text);

signals:
    // A field of the component was edited by hand.
    void componentEdited(const QString &uid);

private slots:
    void onFieldEdited();
    void onSearchChanged();
    void onAddFact();
    void onRemoveFact();
    void onFactActivated(QListWidgetItem *item);

private:
    const Component *component() const;
    Component *editableComponent();
    void showComponent(const QString &uid);
    void showFacts();
    void storeField(const QString &what, const QString &value, QString &target,
                    const QString &provenanceNote);
    void setUpdating(bool updating) { m_updating = updating; }

    Project *m_project = nullptr;
    Database *m_database = nullptr;
    QString m_uid;

    QLabel *m_title = nullptr;
    QLabel *m_summary = nullptr;

    QLineEdit *m_value = nullptr;
    QLineEdit *m_description = nullptr;
    QComboBox *m_category = nullptr;
    QLineEdit *m_manufacturer = nullptr;
    QLineEdit *m_mpn = nullptr;
    QLineEdit *m_package = nullptr;
    QLineEdit *m_datasheetUrl = nullptr;
    QLineEdit *m_datasheetPath = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QLabel *m_provenance = nullptr;

    QTreeWidget *m_pads = nullptr;

    QLineEdit *m_search = nullptr;
    QListWidget *m_facts = nullptr;
    QLabel *m_factStatus = nullptr;
    QHash<int, qlonglong> m_factIds;   // list row -> fact id

    bool m_updating = false;
};

} // namespace eda
