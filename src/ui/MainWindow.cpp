#include "MainWindow.h"

#include "AssistantPanel.h"
#include "BoardView.h"
#include "ComponentDetails.h"
#include "DiagnosticsPanel.h"
#include "ComponentExplorer.h"
#include "LogPanel.h"
#include "SettingsDialog.h"
#include "TestPointPanel.h"
#include "Theme.h"

#include "../application/Application.h"
#include "../application/AppIdentity.h"
#include "../application/Settings.h"
#include "../core/KicadImport.h"
#include "../core/Logging.h"
#include "../core/Paths.h"
#include "../core/Project.h"
#include <eda/Version.h>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QStatusBar>
#include <QUuid>
#include <QToolBar>
#include <QUrl>

namespace eda {
namespace {

QString workspaceFile() { return Paths::workspaceStateFile(); }

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setObjectName(QStringLiteral("MainWindow"));
    setWindowTitle(Application::instance()->identity()->windowTitle());

    createActions();
    createMenus();
    createToolbars();
    createDocks();
    createStatusBar();

    connectProjectSignals();
    updateWindowTitle();
    updateActionStates();
    restoreWorkspace();
    offerRecovery();

    connect(Application::instance(), &Application::identityChanged, this,
            &MainWindow::updateWindowTitle);
}

MainWindow::~MainWindow() { saveWorkspace(); }

QAction *MainWindow::action(const QString &objectName) const
{
    return m_actions.value(objectName);
}

// ---------------------------------------------------------------------------
// actions, menus, toolbars
// ---------------------------------------------------------------------------
void MainWindow::createActions()
{
    auto add = [this](const QString &id, const QString &text, const QString &iconText,
                      const QKeySequence &shortcut, auto slot) -> QAction * {
        auto *action = new QAction(text, this);
        action->setObjectName(id);
        action->setShortcut(shortcut);
        if (!iconText.isEmpty())
            action->setIconText(iconText);
        connect(action, &QAction::triggered, this, slot);
        m_actions.insert(id, action);
        return action;
    };

    add(QStringLiteral("file.new"), tr("&New Project"), tr("New"), QKeySequence::New,
        [this]() { newProject(); });
    add(QStringLiteral("file.open"), tr("&Open Project..."), tr("Open"), QKeySequence::Open,
        [this]() { openProject(); });
    add(QStringLiteral("file.importBoard"), tr("&Import Board File..."), tr("Import"),
        QKeySequence(Qt::ControlModifier | Qt::Key_I), [this]() { importBoardFile(); });
    add(QStringLiteral("file.save"), tr("&Save"), tr("Save"), QKeySequence::Save,
        [this]() { saveProject(); });
    add(QStringLiteral("file.saveAs"), tr("Save &As..."), tr("Save As"), QKeySequence::SaveAs,
        [this]() { saveProjectAs(); });
    add(QStringLiteral("file.close"), tr("Close Project"), QString(), QKeySequence(),
        [this]() { closeProject(); });
    add(QStringLiteral("file.exit"), tr("E&xit"), QString(), QKeySequence::Quit,
        [this]() { close(); });

    add(QStringLiteral("edit.findComponent"), tr("&Find Component..."), QString(), QKeySequence::Find,
        [this]() { findComponent(); });
    add(QStringLiteral("edit.clearSelection"), tr("Clear Selection"), QString(), QKeySequence(),
        [this]() { m_explorer->clearSelection(); m_boardView->setSelectedComponent(QString()); });

    add(QStringLiteral("view.zoomIn"), tr("Zoom In"), QStringLiteral("+"), QKeySequence::ZoomIn,
        [this]() { m_boardView->zoomIn(); });
    add(QStringLiteral("view.zoomOut"), tr("Zoom Out"), QStringLiteral("-"), QKeySequence::ZoomOut,
        [this]() { m_boardView->zoomOut(); });
    add(QStringLiteral("view.fit"), tr("Fit Board to View"), QStringLiteral("Fit"),
        QKeySequence(Qt::Key_F), [this]() { m_boardView->fitToBoard(); });
    add(QStringLiteral("view.reset"), tr("Reset View"), QStringLiteral("1:1"), QKeySequence(Qt::Key_0),
        [this]() { m_boardView->resetView(); });
    auto *backSide = add(QStringLiteral("view.sideBottom"), tr("Show Board &Back"), QString(),
                         QKeySequence(Qt::Key_B), [this](bool checked) {
                             showBoardSide(checked ? BoardSide::Bottom : BoardSide::Top);
                         });
    backSide->setCheckable(true);
    backSide->setToolTip(tr("Mirror the board the way it looks when you turn it over."));
    add(QStringLiteral("view.resetLayout"), tr("Reset Workspace Layout"), QString(), QKeySequence(),
        [this]() { resetWorkspaceLayout(); });

    add(QStringLiteral("tools.openLogDirectory"), tr("Open Log Directory"), QString(), QKeySequence(),
        [this]() { openLogDirectory(); });

    add(QStringLiteral("settings.application"), tr("&Settings..."), QString(),
        QKeySequence::Preferences, [this]() { showSettings(0); });
    add(QStringLiteral("settings.appearance"), tr("&Appearance..."), QString(), QKeySequence(),
        [this]() { showSettings(1); });
    add(QStringLiteral("settings.ai"), tr("Configure &AI Provider..."), QString(), QKeySequence(),
        [this]() { showSettings(4); });

    add(QStringLiteral("help.manual"), tr("User &Manual"), QString(), QKeySequence::HelpContents,
        [this]() { openUserManual(); });
    add(QStringLiteral("help.about"), tr("&About"), QString(), QKeySequence(),
        [this]() { showAbout(); });
    add(QStringLiteral("help.aboutQt"), tr("About &Qt"), QString(), QKeySequence(),
        [this]() { showAboutQt(); });
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_actions.value(QStringLiteral("file.new")));
    fileMenu->addAction(m_actions.value(QStringLiteral("file.open")));
    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("file.importBoard")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("file.save")));
    fileMenu->addAction(m_actions.value(QStringLiteral("file.saveAs")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("file.close")));
    fileMenu->addSeparator();
    fileMenu->addAction(m_actions.value(QStringLiteral("file.exit")));

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_actions.value(QStringLiteral("edit.findComponent")));
    editMenu->addAction(m_actions.value(QStringLiteral("edit.clearSelection")));

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_actions.value(QStringLiteral("view.zoomIn")));
    viewMenu->addAction(m_actions.value(QStringLiteral("view.zoomOut")));
    viewMenu->addAction(m_actions.value(QStringLiteral("view.fit")));
    viewMenu->addAction(m_actions.value(QStringLiteral("view.reset")));
    viewMenu->addAction(m_actions.value(QStringLiteral("view.sideBottom")));
    viewMenu->addSeparator();
    auto *panels = viewMenu->addMenu(tr("Panels"));
    viewMenu->addSeparator();
    viewMenu->addAction(m_actions.value(QStringLiteral("view.resetLayout")));

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(m_actions.value(QStringLiteral("tools.openLogDirectory")));

    auto *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(m_actions.value(QStringLiteral("settings.application")));
    settingsMenu->addAction(m_actions.value(QStringLiteral("settings.appearance")));
    settingsMenu->addAction(m_actions.value(QStringLiteral("settings.ai")));

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(m_actions.value(QStringLiteral("help.manual")));
    helpMenu->addSeparator();
    helpMenu->addAction(m_actions.value(QStringLiteral("help.about")));
    helpMenu->addAction(m_actions.value(QStringLiteral("help.aboutQt")));

    // The panel visibility actions are created together with the docks.
    m_panelMenu = panels;
}

void MainWindow::createToolbars()
{
    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setObjectName(QStringLiteral("MainToolbar"));
    toolbar->setMovable(true);
    toolbar->addAction(m_actions.value(QStringLiteral("file.new")));
    toolbar->addAction(m_actions.value(QStringLiteral("file.open")));
    toolbar->addAction(m_actions.value(QStringLiteral("file.importBoard")));
    toolbar->addAction(m_actions.value(QStringLiteral("file.save")));
    toolbar->addSeparator();
    toolbar->addAction(m_actions.value(QStringLiteral("view.zoomIn")));
    toolbar->addAction(m_actions.value(QStringLiteral("view.zoomOut")));
    toolbar->addAction(m_actions.value(QStringLiteral("view.fit")));
    toolbar->addSeparator();
    toolbar->addAction(m_actions.value(QStringLiteral("settings.application")));
}

void MainWindow::createDocks()
{
    m_explorer = new ComponentExplorer(this);
    auto *explorerDock = new QDockWidget(tr("Component Explorer"), this);
    explorerDock->setObjectName(QStringLiteral("ComponentExplorerDock"));
    explorerDock->setWidget(m_explorer);
    addDockWidget(Qt::LeftDockWidgetArea, explorerDock);
    m_docks.append(explorerDock);

    m_boardView = new BoardView(this);
    m_boardView->setBoard(nullptr);
    setCentralWidget(m_boardView);
    connect(m_boardView, &BoardView::componentClicked, this, [this](const QString &uid) {
        m_explorer->selectComponent(uid);
        m_details->selectComponent(uid);
    });
    connect(m_explorer, &ComponentExplorer::componentSelected, this, [this](const QString &uid) {
        m_boardView->setSelectedComponent(uid);
        m_details->selectComponent(uid);
    });
    connect(m_boardView, &BoardView::cursorPositionChanged, this, [this](const QPointF &position) {
        m_coordinateState->setText(tr("X %1 mm  Y %2 mm")
                                       .arg(position.x(), 0, 'f', 2)
                                       .arg(position.y(), 0, 'f', 2));
    });
    connect(m_boardView, &BoardView::zoomChanged, this, [this](double factor) {
        if (m_actions.contains(QStringLiteral("view.zoomIn"))) {
            Q_UNUSED(factor)
        }
    });

    m_details = new ComponentDetails(this);
    m_details->setProject(project());
    m_details->setDatabase(Application::instance()->database());
    auto *detailsDock = new QDockWidget(tr("Component Details"), this);
    detailsDock->setObjectName(QStringLiteral("ComponentDetailsDock"));
    detailsDock->setWidget(m_details);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);
    m_docks.append(detailsDock);
    connect(m_details, &ComponentDetails::componentEdited, this, [this](const QString &) {
        // The explorer reads its values from the model, so it has to be told
        // that one of its rows changed.
        project()->markDirty();
        m_explorer->setBoard(project()->primaryBoard());
        updateProjectState();
    });

    m_assistant = new AssistantPanel(this);
    auto *assistantDock = new QDockWidget(tr("Diagnostic Assistant"), this);
    assistantDock->setObjectName(QStringLiteral("AssistantDock"));
    assistantDock->setWidget(m_assistant);
    addDockWidget(Qt::RightDockWidgetArea, assistantDock);
    m_docks.append(assistantDock);
    connect(m_assistant, &AssistantPanel::configureRequested, this, [this]() { showSettings(4); });

    m_testPoints = new TestPointPanel(this);
    m_testPoints->setProject(project());
    auto *testPointDock = new QDockWidget(tr("Test Points"), this);
    testPointDock->setObjectName(QStringLiteral("TestPointDock"));
    testPointDock->setWidget(m_testPoints);
    addDockWidget(Qt::BottomDockWidgetArea, testPointDock);
    m_docks.append(testPointDock);
    connect(m_testPoints, &TestPointPanel::testPointsChanged, this, [this]() {
        project()->markDirty();
        updateProjectState();
    });
    // A test point is usually taken on the component that is being looked at.
    connect(m_explorer, &ComponentExplorer::componentSelected, m_testPoints,
            &TestPointPanel::setContextComponent);

    m_diagnostics = new DiagnosticsPanel(this);
    m_diagnostics->setProject(project());
    auto *diagnosticsDock = new QDockWidget(tr("Diagnostics"), this);
    diagnosticsDock->setObjectName(QStringLiteral("DiagnosticsDock"));
    diagnosticsDock->setWidget(m_diagnostics);
    tabifyDockWidget(testPointDock, diagnosticsDock);
    m_docks.append(diagnosticsDock);
    connect(m_diagnostics, &DiagnosticsPanel::hypothesesChanged, this, [this]() {
        project()->markDirty();
        updateProjectState();
    });
    // A hypothesis is usually about the component that is being looked at.
    connect(m_explorer, &ComponentExplorer::componentSelected, m_diagnostics,
            &DiagnosticsPanel::setContextComponent);
    // A new reading can change what the data suggests, and vice versa.
    connect(m_testPoints, &TestPointPanel::testPointsChanged, m_diagnostics,
            &DiagnosticsPanel::refresh);
    // Readings taken while a session is open belong to that session.
    connect(m_diagnostics, &DiagnosticsPanel::activeSessionChanged, m_testPoints,
            &TestPointPanel::setSessionId);

    m_logPanel = new LogPanel(this);
    auto *logDock = new QDockWidget(tr("Log"), this);
    logDock->setObjectName(QStringLiteral("LogDock"));
    logDock->setWidget(m_logPanel);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    m_docks.append(logDock);

    if (m_panelMenu) {
        for (QDockWidget *dock : m_docks)
            m_panelMenu->addAction(dock->toggleViewAction());
    }
}

void MainWindow::createStatusBar()
{
    m_projectState = new QLabel(this);
    m_projectState->setMinimumWidth(200);
    statusBar()->addWidget(m_projectState, 1);

    m_sideState = new QLabel(tr("Side: %1").arg(tr("front")), this);
    statusBar()->addPermanentWidget(m_sideState);

    m_coordinateState = new QLabel(tr("X 0.00 mm  Y 0.00 mm"), this);
    m_coordinateState->setMinimumWidth(160);
    statusBar()->addPermanentWidget(m_coordinateState);

    m_modelState = new QLabel(this);
    statusBar()->addPermanentWidget(m_modelState);

    m_taskProgress = new QProgressBar(this);
    m_taskProgress->setRange(0, 100);
    m_taskProgress->setFixedWidth(120);
    m_taskProgress->setVisible(false);
    statusBar()->addPermanentWidget(m_taskProgress);

    connect(m_assistant, &AssistantPanel::configureRequested, this, [this]() { showSettings(4); });

    m_modelState->setText(m_assistant->statusSummary());
    updateProjectState();
}

// ---------------------------------------------------------------------------
// project handling
// ---------------------------------------------------------------------------
void MainWindow::connectProjectSignals()
{
    Project *project = Application::instance()->project();
    connect(project, &Project::opened, this, [this](const QString &path) {
        Application::instance()->settings()->addRecentProject(path);
        m_testPoints->refresh();
        m_diagnostics->refresh();
        m_explorer->setBoard(this->project()->primaryBoard());
        m_boardView->setBoard(this->project()->primaryBoard());
        m_details->selectComponent(QString());
        m_details->refresh();
        updateWindowTitle();
        updateProjectState();
        updateActionStates();
        rebuildRecentMenu();
    });
    connect(project, &Project::saved, this, [this](const QString &path) {
        Application::instance()->settings()->addRecentProject(path);
        updateWindowTitle();
        updateProjectState();
        rebuildRecentMenu();
    });
    connect(project, &Project::closed, this, [this]() {
        m_testPoints->refresh();
        m_diagnostics->refresh();
        m_explorer->setBoard(nullptr);
        m_boardView->setBoard(nullptr);
        m_details->selectComponent(QString());
        m_details->refresh();
        updateWindowTitle();
        updateProjectState();
        updateActionStates();
    });
    connect(project, &Project::dirtyChanged, this, [this](bool) {
        updateWindowTitle();
        updateProjectState();
    });
    connect(project, &Project::dataChanged, this, [this]() {
        m_testPoints->refresh();
        m_diagnostics->refresh();
        m_explorer->setBoard(this->project()->primaryBoard());
        m_boardView->setBoard(this->project()->primaryBoard());
        m_details->refresh();
    });
}

Project *MainWindow::project() const { return Application::instance()->project(); }

void MainWindow::newProject()
{
    if (!maybeSave())
        return;

    const QString directory = projectDirectory();
    bool accepted = false;
    const QString name = QInputDialog::getText(this, tr("New Project"), tr("Project name:"),
                                               QLineEdit::Normal, tr("Untitled project"), &accepted);
    if (!accepted || name.trimmed().isEmpty())
        return;

    QString error;
    if (!project()->create(name, directory, &error)) {
        QMessageBox::critical(this, tr("New Project"), error);
        return;
    }
    logInfo(QStringLiteral("ui"), QStringLiteral("project created: %1").arg(project()->filePath()));
}

void MainWindow::openProject()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Project"), projectDirectory(),
                                                      Project::fileFilter());
    if (path.isEmpty())
        return;
    openProjectPath(path);
}

void MainWindow::openProjectPath(const QString &path)
{
    QString error;
    if (!project()->open(path, &error)) {
        QMessageBox::critical(this, tr("Open Project"), error);
        return;
    }
}

void MainWindow::importBoardFile()
{
    if (!project()->isOpen()) {
        QMessageBox::information(this, tr("Import Board File"),
                                 tr("Create or open a project first: an imported board is stored "
                                    "inside a project, together with the notes and measurements "
                                    "that belong to it."));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, tr("Import Board File"),
                                                      projectDirectory(), eda::kicadBoardFileFilter());
    if (path.isEmpty())
        return;
    QString error;
    if (!importBoardFile(path, &error)) {
        QMessageBox::critical(this, tr("Import Board File"),
                              tr("The board file could not be imported.\n\n%1\n\n%2")
                                  .arg(QDir::toNativeSeparators(path), error));
    }
}

bool MainWindow::importBoardFile(const QString &path, QString *error)
{
    if (path.isEmpty())
        return false;
    Project *prj = project();
    if (!prj->isOpen()) {
        if (error)
            *error = tr("There is no open project to import into.");
        return false;
    }

    const QFileInfo info(path);
    beginTask(tr("Importing %1").arg(info.fileName()));
    QString importError;
    const KicadImportResult result = importKicadPcb(path, &importError);
    endTask();

    if (!result.ok) {
        logError(QStringLiteral("import"),
                 tr("Could not import %1: %2").arg(info.fileName(), importError));
        if (error)
            *error = importError;
        return false;
    }
    if (error)
        error->clear();

    Board board = result.board;
    // The importer reads geometry; anything it could not read is listed in the
    // board's warnings, and the user sees them here.
    for (const QString &warning : board.warnings)
        logWarning(QStringLiteral("import"), tr("%1: %2").arg(info.fileName(), warning));

    ImportedDocument document;
    document.uid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    document.path = path;
    document.format = board.sourceFormat;
    document.title = board.name.isEmpty() ? info.fileName() : board.name;
    document.imported = QDateTime::currentDateTimeUtc();
    document.warnings = board.warnings;

    if (prj->primaryBoard() == nullptr)
        prj->data().boards.append(board);
    else
        *prj->primaryBoard() = board;
    prj->data().documents.append(document);
    prj->markDirty();

    // What the file says about its parts is reusable knowledge: it is recorded
    // once, with the board file as its source.
    const int recorded = recordComponentsInDatabase(*prj->primaryBoard());

    m_explorer->setBoard(prj->primaryBoard());
    m_boardView->setBoard(prj->primaryBoard());
    m_boardView->fitToBoard();
    m_details->selectComponent(QString());
    m_details->refresh();
    updateProjectState();
    updateActionStates();

    statusBar()->showMessage(
        tr("Imported %1: %2 components, %3 pads, %4 nets, %5 tracks%6%7")
            .arg(info.fileName())
            .arg(result.componentCount)
            .arg(result.padCount)
            .arg(prj->primaryBoard()->nets.size())
            .arg(result.trackCount)
            .arg(board.warnings.isEmpty()
                     ? QString()
                     : tr(" (%n thing(s) in the file were not imported, see the log)", "",
                          board.warnings.size()))
            .arg(recorded > 0 ? tr(", %n part number(s) recorded in the knowledge base", "",
                                   recorded)
                              : QString()),
        10000);
    logInfo(QStringLiteral("import"),
            tr("Imported %1 (%2 components)").arg(path).arg(result.componentCount));
    return true;
}

bool MainWindow::saveProject()
{
    if (!project()->isOpen()) {
        QMessageBox::information(this, tr("Save"), tr("There is no open project."));
        return false;
    }
    QString error;
    if (!project()->save(&error)) {
        QMessageBox::critical(this, tr("Save Project"), error);
        return false;
    }
    return true;
}

bool MainWindow::saveProjectAs()
{
    if (!project()->isOpen()) {
        QMessageBox::information(this, tr("Save As"), tr("There is no open project."));
        return false;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Project As"),
                                                      project()->filePath(), Project::fileFilter());
    if (path.isEmpty())
        return false;
    QString error;
    if (!project()->saveAs(path, &error)) {
        QMessageBox::critical(this, tr("Save Project As"), error);
        return false;
    }
    return true;
}

bool MainWindow::closeProject()
{
    if (!project()->isOpen())
        return true;
    if (!maybeSave())
        return false;
    QString error;
    if (!project()->close(&error)) {
        QMessageBox::critical(this, tr("Close Project"), error);
        return false;
    }
    return true;
}

bool MainWindow::maybeSave()
{
    Project *prj = project();
    if (!prj->isOpen() || !prj->isDirty())
        return true;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Unsaved Changes"),
        tr("The project \"%1\" has unsaved changes. Do you want to save them?")
            .arg(prj->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    if (answer == QMessageBox::Discard)
        return true;
    return false;
}

void MainWindow::offerRecovery()
{
    Project *prj = project();
    if (!prj->hasRecoveryData())
        return;

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Recover Unsaved Work"),
        tr("The application found unsaved work from a previous session. Do you want to "
           "restore it?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes) {
        QString error;
        if (!prj->restoreFromRecovery(&error))
            QMessageBox::warning(this, tr("Recover Unsaved Work"), error);
    } else {
        prj->discardRecoveryData();
    }
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu)
        return;
    m_recentMenu->clear();
    const QStringList projects = Application::instance()->settings()->recentProjects();
    if (projects.isEmpty()) {
        QAction *empty = m_recentMenu->addAction(tr("(no recent projects)"));
        empty->setEnabled(false);
        return;
    }
    for (const QString &path : projects) {
        QAction *action = m_recentMenu->addAction(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path]() {
            if (!maybeSave())
                return;
            openProjectPath(path);
        });
    }
    m_recentMenu->addSeparator();
    QAction *clear = m_recentMenu->addAction(tr("Clear list"));
    connect(clear, &QAction::triggered, this, [this]() {
        Application::instance()->settings()->clearRecentProjects();
        rebuildRecentMenu();
    });
}

// ---------------------------------------------------------------------------
// misc slots
// ---------------------------------------------------------------------------
void MainWindow::showSettings(int pageIndex)
{
    auto *dialog = new SettingsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCurrentPage(pageIndex);
    connect(dialog, &SettingsDialog::settingsChanged, this, [this]() {
        const QColor accent = Application::instance()->identity()->identity().accent;
        const QColor secondary = Application::instance()->identity()->identity().accentSecondary;
        applyTheme(qobject_cast<QApplication *>(QApplication::instance()),
                   themeColors(accent, secondary));
        m_assistant->refreshFromSettings();
        m_modelState->setText(m_assistant->statusSummary());
        Logger::instance().setMinimumLevel(logLevelFromName(
            Application::instance()->settings()
                ->value(QStringLiteral("logging/level"), QStringLiteral("info")).toString()));
    });
    dialog->exec();
}

void MainWindow::showAbout()
{
    const Identity identity = Application::instance()->identity()->identity();
    QMessageBox box(this);
    box.setWindowTitle(tr("About"));
    box.setIconPixmap(Application::instance()->identity()->icon().pixmap(64, 64));
    box.setText(QStringLiteral("<h3>%1</h3><p>%2</p>").arg(identity.displayName().toHtmlEscaped(),
                                                           identity.subtitle.toHtmlEscaped()));
    box.setInformativeText(
        tr("<table>"
           "<tr><td>Version:</td><td>%1</td></tr>"
           "<tr><td>Qt:</td><td>%2 (runtime %3)</td></tr>"
           "<tr><td>Configuration:</td><td>%4</td></tr>"
           "<tr><td>Log file:</td><td>%5</td></tr>"
           "<tr><td>Projects:</td><td>%6</td></tr>"
           "</table>")
            .arg(QStringLiteral(EDA_VERSION_STRING), QStringLiteral(EDA_QT_VERSION),
                 QStringLiteral(QT_VERSION_STR), QDir::toNativeSeparators(Paths::settingsFile()),
                 QDir::toNativeSeparators(Logger::instance().currentLogFile()),
                 QDir::toNativeSeparators(Paths::defaultProjectDir())));
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void MainWindow::showAboutQt() { QMessageBox::aboutQt(this); }

void MainWindow::openUserManual()
{
    const QString manual = QStringLiteral(EDA_MANUAL_PATH);
    if (!QFileInfo::exists(manual)) {
        QMessageBox::warning(this, tr("User Manual"),
                             tr("The user manual is not installed here: %1").arg(manual));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(manual));
}

void MainWindow::openLogDirectory()
{
    const QString dir = Logger::instance().logDirectory();
    if (!QFileInfo::exists(dir))
        QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void MainWindow::findComponent()
{
    auto *dock = findChild<QDockWidget *>(QStringLiteral("ComponentExplorerDock"));
    if (dock)
        dock->raise();
    m_explorer->setFocus();
    m_explorer->setFilterText(QString());
}

void MainWindow::resetWorkspaceLayout()
{
    QSettings settings(workspaceFile(), QSettings::IniFormat);
    settings.remove(QStringLiteral("geometry"));
    settings.remove(QStringLiteral("state"));
    for (QDockWidget *dock : m_docks) {
        dock->setFloating(false);
        dock->setVisible(true);
    }
    addDockWidget(Qt::LeftDockWidgetArea, m_docks.at(0));
    addDockWidget(Qt::RightDockWidgetArea, m_docks.at(1));
    addDockWidget(Qt::BottomDockWidgetArea, m_docks.at(2));
    // Everything else shares the bottom area as tabs: the explorer and the
    // details panel are the two panels that are wanted all the time.
    for (int i = 3; i < m_docks.size(); ++i)
        tabifyDockWidget(m_docks.at(2), m_docks.at(i));
    m_docks.at(2)->raise();
    resize(1280, 800);
    logInfo(QStringLiteral("ui"), QStringLiteral("workspace layout reset"));
}

// ---------------------------------------------------------------------------
// background task feedback
// ---------------------------------------------------------------------------
void MainWindow::beginTask(const QString &description)
{
    m_taskProgress->setVisible(true);
    m_taskProgress->setValue(0);
    statusBar()->showMessage(description, 5000);
    QApplication::processEvents();
}

void MainWindow::setTaskProgress(int percent) { m_taskProgress->setValue(qBound(0, percent, 100)); }

void MainWindow::endTask()
{
    m_taskProgress->setVisible(false);
    m_taskProgress->setValue(0);
    statusBar()->clearMessage();
}

// ---------------------------------------------------------------------------
// workspace and state
// ---------------------------------------------------------------------------
void MainWindow::restoreWorkspace()
{
    QSettings settings(workspaceFile(), QSettings::IniFormat);
    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("state")).toByteArray();
    if (geometry.isEmpty() || state.isEmpty()) {
        resize(1280, 800);
        return;
    }
    restoreGeometry(geometry);
    restoreState(state);
}

void MainWindow::saveWorkspace()
{
    QSettings settings(workspaceFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("state"), saveState());
    settings.sync();
}

void MainWindow::updateWindowTitle()
{
    const Project *prj = project();
    QString document;
    if (prj->isOpen())
        document = prj->displayName() + (prj->isDirty() ? QStringLiteral("*") : QString());
    setWindowTitle(Application::instance()->identity()->windowTitle(document));
}

int MainWindow::recordComponentsInDatabase(const Board &board)
{
    Database *database = Application::instance()->database();
    if (database == nullptr || !database->isOpen())
        return 0;

    int recorded = 0;
    int failed = 0;
    for (const Component &component : board.components) {
        // Only a part the file actually names can be recorded. A component that
        // is nothing but "100n" on a board is not a part number, and turning it
        // into one would be inventing data.
        if (component.manufacturerPartNumber.trimmed().isEmpty())
            continue;

        ComponentRecord record;
        record.mpn = component.manufacturerPartNumber.trimmed();
        record.manufacturer = component.manufacturer;
        record.category = component.category;
        record.description = component.description.isEmpty() ? component.value
                                                             : component.description;
        record.datasheetUrl = component.datasheetUrl;
        record.datasheetPath = component.datasheetPath;
        record.provenance.title = board.name;
        record.provenance.localPath = board.sourcePath;
        record.provenance.location = QStringLiteral("footprint %1").arg(component.reference);
        record.provenance.status = VerificationStatus::ExtractedFromSource;
        record.provenance.valueSource = ValueSource::ExtractedFromSource;
        record.provenance.retrieved = QDateTime::currentDateTimeUtc();

        QString error;
        if (database->upsertComponent(&record, &error)) {
            ++recorded;
        } else {
            ++failed;
            if (failed == 1)
                logError(QStringLiteral("database"), error);
        }
    }
    if (failed > 1) {
        logError(QStringLiteral("database"),
                 tr("%1 part numbers could not be recorded in the knowledge database.")
                     .arg(failed));
    }
    if (recorded > 0) {
        logInfo(QStringLiteral("database"),
                tr("%n part number(s) recorded in the knowledge database.", "", recorded));
    }
    return recorded;
}

void MainWindow::showBoardSide(BoardSide side)
{
    m_boardView->setSide(side);
    QAction *action = m_actions.value(QStringLiteral("view.sideBottom"));
    if (action)
        action->setChecked(side == BoardSide::Bottom);
    m_sideState->setText(tr("Side: %1").arg(side == BoardSide::Bottom ? tr("back")
                                                                     : tr("front")));
}

void MainWindow::updateActionStates()
{
    const bool open = project()->isOpen();
    m_actions.value(QStringLiteral("file.save"))->setEnabled(open);
    m_actions.value(QStringLiteral("file.saveAs"))->setEnabled(open);
    m_actions.value(QStringLiteral("file.close"))->setEnabled(open);
    m_actions.value(QStringLiteral("file.importBoard"))->setEnabled(open);
    m_actions.value(QStringLiteral("edit.findComponent"))->setEnabled(open);
    m_actions.value(QStringLiteral("view.sideBottom"))
        ->setEnabled(m_boardView != nullptr && m_boardView->board() != nullptr);
    rebuildRecentMenu();
}

void MainWindow::updateProjectState()
{
    const Project *prj = project();
    if (!prj->isOpen()) {
        m_projectState->setText(tr("No project open"));
        return;
    }
    QString text = tr("%1%2").arg(prj->displayName(),
                                  prj->isDirty() ? tr(" (unsaved changes)") : QString());
    if (m_diagnostics != nullptr && !prj->data().hypotheses.isEmpty())
        text += tr("  |  %1").arg(m_diagnostics->summaryText());
    if (m_testPoints != nullptr && !prj->data().testPoints.isEmpty()) {
        const TestCoverage coverage = m_testPoints->coverage();
        text += tr("  |  %n test point(s), %1 measured, %2 pass, %3 fail", "",
                   coverage.testPoints)
                    .arg(coverage.measured)
                    .arg(coverage.passing)
                    .arg(coverage.failing);
    }
    const Board *board = prj->primaryBoard();
    if (board != nullptr && !board->components.isEmpty()) {
        text += tr("  |  %1 components, %2 nets").arg(board->components.size())
                                                  .arg(board->nets.size());
        if (!board->sourcePath.isEmpty())
            text += tr("  |  %1").arg(QFileInfo(board->sourcePath).fileName());
    } else if (board != nullptr && !board->sourcePath.isEmpty()) {
        text += tr("  |  %1 (no components)").arg(QFileInfo(board->sourcePath).fileName());
    } else {
        text += tr("  |  no board imported");
    }
    m_projectState->setText(text);
}

QString MainWindow::projectDirectory() const
{
    Settings *settings = Application::instance()->settings();
    QString directory = settings->value(QStringLiteral("paths/projectDirectory")).toString();
    if (directory.isEmpty())
        directory = Paths::defaultProjectDir();
    if (project()->isOpen() && !project()->directory().isEmpty())
        directory = project()->directory();
    return directory;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    saveWorkspace();
    event->accept();
}

} // namespace eda
