// Bottom panel: test points and the measurements taken at them.
//
// A test point is where the probe goes and what the board should read there.
// The panel keeps the two apart: the expectation (with its source) belongs to
// the test point, the reading belongs to the measurement, and the verdict is
// computed when the reading is taken -- never guessed before, never silently
// changed afterwards.
//
// Every operation the buttons perform is also available without a dialog
// (addTestPoint, recordMeasurement, removeTestPoint), so the panel can be driven
// from a script or a test.
#pragma once

#include <QWidget>

#include "../core/Measurements.h"
#include "../core/ProjectModel.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace eda {

class Project;

class TestPointPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TestPointPanel(QWidget *parent = nullptr);

    void setProject(Project *project);
    // The component that "Add..." proposes, usually the one selected elsewhere.
    void setContextComponent(const QString &uid);

    int visibleCount() const;
    QString selectedTestPointUid() const;
    void selectTestPoint(const QString &uid);

    // Returns the uid of what was stored, or an empty string with <error> set.
    QString addTestPoint(const TestPoint &point, QString *error = nullptr);
    QString recordMeasurement(const QString &testPointUid, double value, const QString &unit,
                              const QString &instrument = QString(),
                              const QString &conditions = QString(),
                              const QString &notes = QString(), QString *error = nullptr);
    bool removeTestPoint(const QString &uid, QString *error = nullptr);
    TestCoverage coverage() const;

public slots:
    // Re-reads the project: the panel keeps no copy of the data.
    void refresh();

signals:
    void testPointsChanged();
    void testPointSelected(const QString &uid);

private slots:
    void onAdd();
    void onMeasure();
    void onRemove();
    void onSelectionChanged();
    void onFilterChanged();

private:
    const QVector<TestPoint> &points() const;
    const TestPoint *testPointByUid(const QString &uid) const;
    void showMeasurements(const QString &testPointUid);
    bool passesFilter(const TestPoint &point) const;

    Project *m_project = nullptr;
    QString m_contextComponent;
    QString m_selected;

    QTreeWidget *m_tree = nullptr;
    QTreeWidget *m_measurements = nullptr;
    QComboBox *m_filter = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_measureButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};

} // namespace eda
