// Main window: menus, toolbars, dockable panels, status bar and workspace
// persistence. Every menu entry is wired to an operation that exists; controls
// for features that are not implemented yet are simply not created.
#pragma once

#include <QMainWindow>

#include "../core/ProjectModel.h"

#include <QHash>
#include <QList>

class QAction;
class QDockWidget;
class QLabel;
class QMenu;
class QProgressBar;
class QSettings;

namespace eda {

class AssistantPanel;
class BoardView;
class ComponentDetails;
class ComponentExplorer;
class LogPanel;
class Project;
class DiagnosticsPanel;
class TestPointPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Test and automation hooks.
    BoardView *boardView() const { return m_boardView; }
    ComponentExplorer *componentExplorer() const { return m_explorer; }
    ComponentDetails *componentDetails() const { return m_details; }
    TestPointPanel *testPointPanel() const { return m_testPoints; }
    DiagnosticsPanel *diagnosticsPanel() const { return m_diagnostics; }
    LogPanel *logPanel() const { return m_logPanel; }
    QAction *action(const QString &objectName) const;

    // Background task feedback (owned by the window, used by long operations).
    void beginTask(const QString &description);
    void setTaskProgress(int percent);
    void endTask();

public slots:
    // Importing a board file. The overload that takes a path does the work and
    // reports failure through <error>; it never opens a dialog, so it can be
    // driven from a script or a test. The overload without arguments asks for a
    // file and is the one that talks to the user.
    bool importBoardFile(const QString &path, QString *error = nullptr);
    void importBoardFile();

    void newProject();
    void openProject();
    void openProjectPath(const QString &path);
    bool saveProject();
    bool saveProjectAs();
    bool closeProject();
    void showSettings(int pageIndex = 0);
    void showAbout();
    void showAboutQt();
    void openUserManual();
    void openLogDirectory();
    void findComponent();
    void resetWorkspaceLayout();
    void showBoardSide(eda::BoardSide side);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    bool maybeSave();
    // Puts every part the board file names into the knowledge database, so that
    // what was imported once is available for the next board too. Returns how
    // many part numbers were recorded.
    int recordComponentsInDatabase(const Board &board);
    void createActions();
    void createMenus();
    void createToolbars();
    void createDocks();
    void createStatusBar();
    void restoreWorkspace();
    void saveWorkspace();
    void updateWindowTitle();
    void updateActionStates();
    void updateProjectState();
    void connectProjectSignals();
    void offerRecovery();
    void rebuildRecentMenu();
    QString projectDirectory() const;

    Project *project() const;

    BoardView *m_boardView = nullptr;
    ComponentExplorer *m_explorer = nullptr;
    ComponentDetails *m_details = nullptr;
    TestPointPanel *m_testPoints = nullptr;
    DiagnosticsPanel *m_diagnostics = nullptr;
    AssistantPanel *m_assistant = nullptr;
    LogPanel *m_logPanel = nullptr;

    QMenu *m_recentMenu = nullptr;
    QLabel *m_projectState = nullptr;
    QLabel *m_sideState = nullptr;
    QLabel *m_modelState = nullptr;
    QLabel *m_coordinateState = nullptr;
    QProgressBar *m_taskProgress = nullptr;
    QAction *m_taskAction = nullptr;

    QList<QDockWidget *> m_docks;
    QMenu *m_panelMenu = nullptr;
    QHash<QString, QAction *> m_actions;
};

} // namespace eda
