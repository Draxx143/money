// UI tests for the main window workbench.
//
// The tests drive the real widgets with the offscreen platform plugin, so no X
// server is needed. Modal dialogs are avoided on purpose: the tests use the same
// entry points the menu actions use, but with the data supplied directly.
#include <QtTest>

#include "../../src/application/AppIdentity.h"
#include "../../src/application/Application.h"
#include "../../src/core/Database.h"
#include "../../src/core/Measurements.h"
#include "../../src/core/Project.h"
#include "../../src/ui/BoardView.h"
#include "../../src/ui/ComponentDetails.h"
#include "../../src/core/Diagnostics.h"
#include "../../src/ui/ComponentExplorer.h"
#include "../../src/ui/DiagnosticsPanel.h"
#include "../../src/ui/MainWindow.h"
#include "../../src/ui/TestPointPanel.h"
#include "../../src/ui/Theme.h"
#include "../TestEnvironment.h"

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QLineEdit>
#include <QTreeWidget>
#include <QGraphicsItem>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    eda::MainWindow *m_window = nullptr;

private slots:
    void initTestCase()
    {
        eda::TestEnvironment::init(QStringLiteral("mainwindow"));
        eda::TestEnvironment::createApplication();
    }

    void cleanupTestCase() { eda::TestEnvironment::cleanup(); }

    void init() { m_window = new eda::MainWindow(); }
    void cleanup()
    {
        delete m_window;
        m_window = nullptr;

        // A project that is closed while it has unsaved changes leaves recovery
        // data behind, and the next main window would ask about it with a modal
        // dialog -- which nobody can answer on a build server.
        eda::Project *project = eda::Application::instance()->project();
        if (project->isOpen())
            project->close();
        project->discardRecoveryData();
    }

    void windowUsesTheConfiguredIdentity()
    {
        QVERIFY(m_window->windowTitle().contains(
            eda::Application::instance()->identity()->name()));
    }

    void requiredMenusExist()
    {
        const QStringList expected = { QStringLiteral("&File"), QStringLiteral("&Edit"),
                                       QStringLiteral("&View"), QStringLiteral("&Tools"),
                                       QStringLiteral("&Settings"), QStringLiteral("&Help") };
        QStringList present;
        for (QAction *action : m_window->menuBar()->actions())
            present.append(action->text());
        for (const QString &menu : expected)
            QVERIFY2(present.contains(menu), qPrintable(menu));
    }

    void everyMenuActionIsUsableWhenAProjectIsOpen()
    {
        // "No dead menu entries": with a project open (the state in which the
        // workbench is actually used) every entry either opens a submenu or is
        // enabled and therefore does something when clicked.  Actions that are
        // disabled on purpose while no project is loaded are covered by
        // projectActionsFollowTheProjectState().
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("menu test"), eda::TestEnvironment::root(), &error));
        // A board is imported as well: the workbench is used with a board
        // loaded, and an action that needs one has to work in that state.
        QVERIFY(m_window->importBoardFile(boardFixture(QStringLiteral("led-blink.kicad_pcb")),
                                          &error));

        int checked = 0;
        for (QAction *menuAction : m_window->menuBar()->actions()) {
            QVERIFY2(menuAction->menu() != nullptr, qPrintable(menuAction->text()));
            for (QAction *action : menuAction->menu()->actions()) {
                if (action->isSeparator())
                    continue;
                ++checked;
                QVERIFY2(!action->text().isEmpty(), "every action needs a label");
                QVERIFY2(action->isEnabled() || action->menu(),
                         qPrintable(action->text() + QStringLiteral(" is dead")));
            }
        }
        QVERIFY(checked > 8);

        QVERIFY(project->close());
    }

    void requiredDocksExist()
    {
        const QStringList names = { QStringLiteral("ComponentExplorerDock"),
                                    QStringLiteral("AssistantDock"),
                                    QStringLiteral("LogDock") };
        for (const QString &name : names) {
            QDockWidget *dock = m_window->findChild<QDockWidget *>(name);
            QVERIFY2(dock != nullptr, qPrintable(name));
            QVERIFY(dock->widget() != nullptr);
        }
    }

    void projectActionsFollowTheProjectState()
    {
        QAction *save = m_window->action(QStringLiteral("file.save"));
        QAction *saveAs = m_window->action(QStringLiteral("file.saveAs"));
        QAction *closeAction = m_window->action(QStringLiteral("file.close"));
        QVERIFY(save && saveAs && closeAction);

        QVERIFY(!save->isEnabled());
        QVERIFY(!saveAs->isEnabled());
        QVERIFY(!closeAction->isEnabled());

        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("UI test"), eda::TestEnvironment::root(), &error));

        QVERIFY(save->isEnabled());
        QVERIFY(saveAs->isEnabled());
        QVERIFY(closeAction->isEnabled());
        QVERIFY(m_window->windowTitle().contains(QStringLiteral("UI test")));

        QVERIFY(project->close());
        QVERIFY(!save->isEnabled());
    }

    void componentsShowUpInTheExplorerAndSyncWithTheViewer()
    {
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("Sync test"), eda::TestEnvironment::root(), &error));

        eda::Board board;
        board.uid = QStringLiteral("b-1");
        board.name = QStringLiteral("board");
        eda::Component first;
        first.uid = QStringLiteral("c-1");
        first.reference = QStringLiteral("R1");
        first.value = QStringLiteral("10k");
        first.position = { 10, 10 };
        eda::Component second;
        second.uid = QStringLiteral("c-2");
        second.reference = QStringLiteral("C4");
        second.value = QStringLiteral("100n");
        second.position = { 20, 12 };
        board.components.append(first);
        board.components.append(second);
        project->data().boards.append(board);
        emit project->dataChanged();

        eda::ComponentExplorer *explorer = m_window->componentExplorer();
        QCOMPARE(explorer->visibleCount(), 2);

        // Selecting in the explorer selects in the board view.
        explorer->selectComponent(QStringLiteral("c-2"));
        QCOMPARE(explorer->selectedComponentUid(), QStringLiteral("c-2"));
        QCOMPARE(m_window->boardView()->selectedComponentUid(), QStringLiteral("c-2"));

        // Clicking the board view (same hit test the mouse uses) selects in the
        // explorer.
        emit m_window->boardView()->componentClicked(QStringLiteral("c-1"));
        QCOMPARE(explorer->selectedComponentUid(), QStringLiteral("c-1"));

        // Filtering hides rows without losing the selection mapping.
        explorer->setFilterText(QStringLiteral("C4"));
        QCOMPARE(explorer->visibleCount(), 1);
        explorer->setFilterText(QString());
        QCOMPARE(explorer->visibleCount(), 2);

        explorer->clearSelection();
        QVERIFY(explorer->selectedComponentUid().isEmpty());

        QVERIFY(project->close());
    }

    void renamingTheApplicationUpdatesTheWindowTitle()
    {
        eda::AppIdentity *identity = eda::Application::instance()->identity();
        const QString original = identity->name();

        eda::Identity renamed = identity->identity();
        renamed.name = QStringLiteral("Bench 42");
        identity->setIdentity(renamed);
        QVERIFY(m_window->windowTitle().startsWith(QStringLiteral("Bench 42")));

        identity->setIdentity(eda::Identity());
        QVERIFY(m_window->windowTitle().startsWith(original));
    }

    void boardViewZoomAndFit()
    {
        const double start = m_window->boardView()->zoomFactor();
        m_window->boardView()->zoomIn();
        QVERIFY(m_window->boardView()->zoomFactor() > start);
        m_window->boardView()->zoomOut();
        QCOMPARE(qRound(m_window->boardView()->zoomFactor() * 1000.0),
                 qRound(start * 1000.0));
        m_window->boardView()->resetView();
    }

    void themeAppliesTheDocumentedPalette()
    {
        const eda::ThemeColors colors = eda::themeColors(QColor(QStringLiteral("#00D1B2")),
                                                         QColor(QStringLiteral("#4DA3FF")));
        eda::applyTheme(qApp, colors);

        const QPalette palette = QApplication::palette();
        QCOMPARE(palette.color(QPalette::Window), QColor(QStringLiteral("#0B0F14")));
        QCOMPARE(palette.color(QPalette::WindowText), QColor(QStringLiteral("#F1F5F9")));
        QCOMPARE(palette.color(QPalette::Base), QColor(QStringLiteral("#18222E")));
        QCOMPARE(palette.color(QPalette::Button), QColor(QStringLiteral("#121923")));
        QCOMPARE(palette.color(QPalette::Highlight), QColor(QStringLiteral("#00D1B2")));
        QCOMPARE(palette.color(QPalette::Link), QColor(QStringLiteral("#4DA3FF")));
        QCOMPARE(palette.color(QPalette::Disabled, QPalette::WindowText),
                 QColor(QStringLiteral("#94A3B8")));
        QVERIFY(!eda::styleSheet(colors).isEmpty());

        // The accent colour is configuration: changing it changes the palette.
        const eda::ThemeColors other = eda::themeColors(QColor(QStringLiteral("#FF0000")),
                                                        QColor(QStringLiteral("#00FF00")));
        eda::applyTheme(qApp, other);
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), QColor(QStringLiteral("#FF0000")));
        eda::applyTheme(qApp, colors);
    }

    // The fixture lives next to the sources; it is a real KiCad board file, so
    // this test covers the whole path: file -> model -> explorer -> viewer.
    QString boardFixture(const QString &name) const
    {
        return QDir::cleanPath(
            QStringLiteral("%1/../data/%2").arg(QStringLiteral(EDA_TEST_DIR), name));
    }

    void importingABoardFileFillsTheWholeWorkbench()
    {
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("Import test"), eda::TestEnvironment::root(), &error));

        // Importing needs a project: the board alone is not the workspace.
        QString importError = QStringLiteral("not set");
        QVERIFY(m_window->action(QStringLiteral("file.importBoard")) != nullptr);
        QVERIFY(m_window->importBoardFile(boardFixture(QStringLiteral("led-blink.kicad_pcb")),
                                          &importError));
        QVERIFY2(importError.isEmpty(), qPrintable(importError));

        const eda::Board *board = project->primaryBoard();
        QVERIFY(board != nullptr);
        QCOMPARE(board->components.size(), 4);
        QCOMPARE(board->nets.size(), 3);
        QCOMPARE(project->data().documents.size(), 1);
        QCOMPARE(project->data().documents.first().format, QStringLiteral("kicad-pcb"));

        QCOMPARE(m_window->componentExplorer()->visibleCount(), 4);
        QCOMPARE(m_window->boardView()->board(), board);

        // The viewer built real geometry: a substrate, the outline, three
        // tracks, two vias and a group per component.
        QVERIFY2(m_window->boardView()->scene()->items().size() > 10,
                 qPrintable(QString::number(m_window->boardView()->scene()->items().size())));

        // The status bar says what was loaded, not just that something was.
        bool found = false;
        for (QLabel *label : m_window->statusBar()->findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("4 components")))
                found = true;
        }
        QVERIFY2(found, "the status bar has to report the imported board");

        // Fitting a board that is on screen gives a usable zoom level.
        m_window->resize(900, 600);
        m_window->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        m_window->boardView()->fitToBoard();
        QVERIFY(m_window->boardView()->zoomFactor() > 0.05);
        QVERIFY(m_window->boardView()->zoomFactor() <= 400.0);

        // A second import replaces the board instead of adding a second one.
        QVERIFY(m_window->importBoardFile(boardFixture(QStringLiteral("arcs.kicad_pcb")),
                                          &importError));
        QCOMPARE(project->data().boards.size(), 1);
        // The four Edge.Cuts lines of the arcs fixture join into one loop, and
        // the circle stays a loop of its own.
        QCOMPARE(project->primaryBoard()->outlines.size(), 2);

        QVERIFY(project->close());
    }

    void importingABrokenBoardIsRefusedWithoutTouchingTheProject()
    {
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("Broken import"), eda::TestEnvironment::root(),
                                &error));

        QString importError;
        QVERIFY(!m_window->importBoardFile(boardFixture(QStringLiteral("broken.kicad_pcb")),
                                           &importError));
        QVERIFY2(!importError.isEmpty(), "the reason has to be reported");
        QVERIFY(project->primaryBoard() == nullptr); // nothing was invented
        QCOMPARE(project->data().documents.size(), 0);
        QCOMPARE(m_window->componentExplorer()->visibleCount(), 0);

        QVERIFY(project->close());
    }

    void boardViewDrawsRealGeometry()
    {
        // A board built by hand, so the test knows exactly what it expects:
        // two pads 3 mm apart, one track, one via and a rectangular outline.
        eda::Board board;
        board.uid = QStringLiteral("b-geometry");
        board.name = QStringLiteral("geometry");
        eda::Component component;
        component.uid = QStringLiteral("c-1");
        component.reference = QStringLiteral("R1");
        component.position = { 10.0, 10.0 };
        eda::Pad pad;
        pad.id = QStringLiteral("1");
        pad.pin = QStringLiteral("1");
        pad.position = { -1.5, 0.0 };
        pad.size = { 1.0, 1.5 };
        pad.shape = eda::PadShape::Rectangle;
        eda::Pad second = pad;
        second.id = QStringLiteral("2");
        second.pin = QStringLiteral("2");
        second.position = { 1.5, 0.0 };
        component.pads = { pad, second };
        board.components.append(component);
        board.tracks.append(eda::Track{});
        board.tracks.first().points = { { 5.0, 5.0 }, { 15.0, 5.0 } };
        board.tracks.first().width = 0.25;
        board.vias.append(eda::Via{});
        board.vias.first().position = { 20.0, 20.0 };
        board.vias.first().diameter = 0.8;
        board.vias.first().drill = 0.4;
        board.outlines.append({ { 0.0, 0.0 }, { 30.0, 0.0 }, { 30.0, 25.0 }, { 0.0, 25.0 },
                                { 0.0, 0.0 } });
        board.boundingBox = { 0.0, 0.0, 30.0, 25.0 };

        eda::BoardView view;
        view.setBoard(&board);
        view.resize(400, 300);
        view.show();

        QVERIFY(view.scene() != nullptr);
        QVERIFY2(view.scene()->items().size() >= 8,
                 qPrintable(QString::number(view.scene()->items().size())));

        // The scene is the board plus a margin, in millimetres.
        const QRectF sceneRect = view.scene()->sceneRect();
        QVERIFY(sceneRect.contains(QRectF(0.0, 0.0, 30.0, 25.0)));

        // Hit testing finds the component where its pads actually are.
        QCOMPARE(view.componentAt(QPointF(10.0, 10.0)), QStringLiteral("c-1"));
        QVERIFY(view.componentAt(QPointF(1.0, 1.0)).isEmpty());

        view.setSide(eda::BoardSide::Bottom);
        QCOMPARE(eda::toString(view.side()), eda::toString(eda::BoardSide::Bottom));
        view.setSide(eda::BoardSide::Top);

        // An empty board is drawn as "nothing there", not as a fake board.
        eda::Board empty;
        empty.uid = QStringLiteral("b-empty");
        view.setBoard(&empty);
        QVERIFY(view.scene()->items().size() <= 2);

        view.setBoard(nullptr);
        QVERIFY(view.board() == nullptr);
        QVERIFY(!view.scene()->items().isEmpty()); // the "no board loaded" hint
    }

    void showingTheBackMirrorsTheBoardWithoutMovingTheData()
    {
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        QVERIFY(project->create(QStringLiteral("Side test"), eda::TestEnvironment::root(), &error));
        QVERIFY(m_window->importBoardFile(boardFixture(QStringLiteral("led-blink.kicad_pcb")),
                                          &error));

        QAction *back = m_window->action(QStringLiteral("view.sideBottom"));
        QVERIFY(back != nullptr);
        QVERIFY(back->isEnabled()); // there is a board to turn over
        QVERIFY(back->isCheckable());
        QVERIFY(!back->isChecked());

        const eda::Point2D placement = project->primaryBoard()->components.first().position;

        back->trigger();
        QVERIFY(back->isChecked());
        QCOMPARE(eda::toString(m_window->boardView()->side()),
                 eda::toString(eda::BoardSide::Bottom));
        // The view is mirrored, which is what turning the board over looks like.
        QVERIFY(m_window->boardView()->transform().m11() < 0.0);
        QVERIFY(m_window->boardView()->transform().m22() > 0.0);
        // The board itself is untouched: a component stays where the file put it.
        QCOMPARE(project->primaryBoard()->components.first().position.x, placement.x);
        QCOMPARE(project->primaryBoard()->components.first().position.y, placement.y);

        bool sideLabel = false;
        for (QLabel *label : m_window->statusBar()->findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("back")))
                sideLabel = true;
        }
        QVERIFY2(sideLabel, "the status bar has to say which side is shown");

        back->trigger();
        QVERIFY(!back->isChecked());
        QCOMPARE(eda::toString(m_window->boardView()->side()),
                 eda::toString(eda::BoardSide::Top));
        QVERIFY(m_window->boardView()->transform().m11() > 0.0);

        QVERIFY(project->close());
        // Without a board there is nothing to turn over.
        QVERIFY(!back->isEnabled());
    }

    // Imports the fixture board into a fresh project and returns the open
    // project, ready for the panel tests.
    eda::Project *projectWithImportedBoard()
    {
        eda::Project *project = eda::Application::instance()->project();
        QString error;
        if (project->isOpen())
            project->close();
        if (!project->create(QStringLiteral("details test"), eda::TestEnvironment::root(), &error))
            return nullptr;
        if (!m_window->importBoardFile(boardFixture(QStringLiteral("led-blink.kicad_pcb")),
                                       &error)) {
            return nullptr;
        }
        return project;
    }

    void componentDetailsFollowTheSelection()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::ComponentDetails *details = m_window->componentDetails();
        QVERIFY(details != nullptr);
        QVERIFY(details->componentUid().isEmpty());

        const eda::Board *board = project->primaryBoard();
        const QString uid = board->componentByReference(QStringLiteral("C1"))->uid;

        m_window->componentExplorer()->selectComponent(uid);
        QCOMPARE(details->componentUid(), uid);
        QVERIFY2(details->summaryText().contains(QStringLiteral("100n")),
                 qPrintable(details->summaryText()));
        QVERIFY2(details->summaryText().contains(QStringLiteral("GND")),
                 qPrintable(details->summaryText()));
        QVERIFY2(details->summaryText().contains(QStringLiteral("VCC")),
                 qPrintable(details->summaryText()));

        // The pads of the component are listed with their nets.
        auto *pads = details->findChild<QTreeWidget *>(QStringLiteral("details.pads"));
        QVERIFY(pads != nullptr);
        QCOMPARE(pads->topLevelItemCount(), 2);
        QCOMPARE(pads->topLevelItem(0)->text(0), QStringLiteral("1"));
        QCOMPARE(pads->topLevelItem(0)->text(2), QStringLiteral("GND"));
        QCOMPARE(pads->topLevelItem(1)->text(2), QStringLiteral("VCC"));

        // Another component, another panel.
        const QString other = board->componentByReference(QStringLiteral("R1"))->uid;
        m_window->componentExplorer()->selectComponent(other);
        QCOMPARE(details->componentUid(), other);
        QVERIFY(!details->summaryText().contains(QStringLiteral("100n")));

        QVERIFY(project->close());
        QVERIFY(details->componentUid().isEmpty());
    }

    void editingAComponentKeepsTheSelectionAndMarksItAsHandEntered()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        const QString uid = project->primaryBoard()->componentByReference(QStringLiteral("C1"))->uid;
        m_window->componentExplorer()->selectComponent(uid);
        QVERIFY2(project->isDirty(), "importing a board leaves the project unsaved");

        auto *mpn = m_window->componentDetails()->findChild<QLineEdit *>(
            QStringLiteral("details.mpn"));
        QVERIFY(mpn != nullptr);
        QCOMPARE(mpn->text(), QStringLiteral("C0805C104K5RACTU"));   // read from the file
        mpn->setText(QStringLiteral("C0805C104K5RACTU-DG"));
        emit mpn->editingFinished();

        const eda::Component *component = project->primaryBoard()->componentByUid(uid);
        QCOMPARE(component->manufacturerPartNumber, QStringLiteral("C0805C104K5RACTU-DG"));
        QVERIFY(project->isDirty());
        // The value is now part file data and part hand entered data, and says
        // so: it is not presented as confirmed.
        QCOMPARE(eda::toString(component->provenance.valueSource),
                 eda::toString(eda::ValueSource::ManualEntry));
        QCOMPARE(eda::toString(component->provenance.status),
                 eda::toString(eda::VerificationStatus::NeedsReview));

        // Editing a component does not lose what the user was working on.
        QCOMPARE(m_window->componentExplorer()->selectedComponentUid(), uid);
        QCOMPARE(m_window->componentDetails()->componentUid(), uid);

        QVERIFY(project->close());
    }

    void knowledgeAboutAComponentIsShownAndFound()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::Database *database = eda::Application::instance()->database();
        QVERIFY(database != nullptr);
        QVERIFY2(database->isOpen(), qPrintable(database->lastError()));

        // The part numbers the board file named are recorded on import: what was
        // imported once is knowledge for the next board. The search is a
        // substring search, so the exact part number is picked out of the hits.
        const QVector<eda::ComponentRecord> records =
            database->findComponents(QStringLiteral("C0805C104K5RACTU"));
        int exact = 0;
        for (const eda::ComponentRecord &candidate : records)
            if (candidate.mpn == QStringLiteral("C0805C104K5RACTU"))
                ++exact;
        QCOMPARE(exact, 1);   // recorded once, however often the board is imported
        const eda::ComponentRecord record =
            database->findComponents(QStringLiteral("KEMET")).first();
        QCOMPARE(record.mpn, QStringLiteral("C0805C104K5RACTU"));
        QCOMPARE(record.manufacturer, QStringLiteral("KEMET"));
        QVERIFY(record.provenance.localPath.endsWith(QStringLiteral("led-blink.kicad_pcb")));
        QCOMPARE(eda::toString(record.provenance.status),
                 eda::toString(eda::VerificationStatus::ExtractedFromSource));

        const QString uid = project->primaryBoard()->componentByReference(QStringLiteral("C1"))->uid;
        m_window->componentExplorer()->selectComponent(uid);
        eda::ComponentDetails *details = m_window->componentDetails();
        QCOMPARE(details->factCount(), 0);

        eda::Fact fact;
        fact.componentUid = uid;
        fact.topic = QStringLiteral("measurement");
        fact.statement = QStringLiteral("Measured 98.4 nF out of circuit at 1 kHz");
        fact.value = QStringLiteral("98.4");
        fact.unit = QStringLiteral("nF");
        fact.provenance.title = QStringLiteral("bench, LCR meter");
        fact.provenance.status = eda::VerificationStatus::ExperimentallyVerified;
        fact.provenance.valueSource = eda::ValueSource::Measured;
        fact.provenance.retrieved = QDateTime::currentDateTimeUtc();
        QVERIFY2(database->addFact(&fact), qPrintable(database->lastError()));

        details->refresh();
        QCOMPARE(details->factCount(), 1);

        // The search finds it, from anywhere in the database.
        details->setSearchText(QStringLiteral("LCR meter"));
        QCOMPARE(details->factCount(), 1);
        QCOMPARE(database->lastSearchMode(), QStringLiteral("fts5"));
        details->setSearchText(QStringLiteral("something nobody wrote down"));
        QCOMPARE(details->factCount(), 0);
        details->setSearchText(QString());
        QCOMPARE(details->factCount(), 1);

        QVERIFY(database->removeFact(fact.id));
        details->refresh();
        QCOMPARE(details->factCount(), 0);

        QVERIFY(project->close());
    }

    void aMeasurementIsComparedWithTheExpectationOfTheTestPoint()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::TestPointPanel *panel = m_window->testPointPanel();
        QVERIFY(panel != nullptr);
        QCOMPARE(panel->visibleCount(), 0);

        eda::TestPoint point;
        point.identifier = QStringLiteral("VCC");
        point.componentUid = project->primaryBoard()->componentByReference(QStringLiteral("C1"))->uid;
        point.pinOrPad = QStringLiteral("2");
        point.netName = QStringLiteral("VCC");
        point.category = QStringLiteral("voltage");
        point.expectedValue = 5.0;
        point.hasExpectedValue = true;
        point.tolerance = 0.25;
        point.hasTolerance = true;
        point.unit = QStringLiteral("V");
        point.testConditions = QStringLiteral("board powered from USB");
        QString error;
        const QString uid = panel->addTestPoint(point, &error);
        QVERIFY2(!uid.isEmpty(), qPrintable(error));
        QCOMPARE(panel->visibleCount(), 1);
        QCOMPARE(project->data().testPoints.size(), 1);

        auto *tree = panel->findChild<QTreeWidget *>(QStringLiteral("testpoints.tree"));
        QVERIFY(tree != nullptr);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("VCC"));
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("C1"));        // the component
        QCOMPARE(tree->topLevelItem(0)->text(2), QStringLiteral("VCC"));       // the net
        QCOMPARE(tree->topLevelItem(0)->text(3), QStringLiteral("5 V +/-0.25"));   // expectation
        QCOMPARE(tree->topLevelItem(0)->text(5), QStringLiteral("Not tested"));

        // A reading inside the tolerance passes.
        QVERIFY2(!panel->recordMeasurement(uid, 5.1, QStringLiteral("V"),
                                           QStringLiteral("Fluke 87V"),
                                           QStringLiteral("board powered"),
                                           QStringLiteral("probe on C1 pin 2"), &error)
                      .isEmpty(),
                 qPrintable(error));
        QCOMPARE(panel->coverage().measured, 1);
        QCOMPARE(panel->coverage().passing, 1);
        QCOMPARE(panel->coverage().failing, 0);
        QCOMPARE(tree->topLevelItem(0)->text(4), QStringLiteral("5.1 V"));
        QCOMPARE(tree->topLevelItem(0)->text(5), QStringLiteral("Pass"));

        // A reading outside the tolerance fails, and it is the newest one that
        // says what the board does now.
        QVERIFY(!panel->recordMeasurement(uid, 4.5, QStringLiteral("V"),
                                          QStringLiteral("Fluke 87V"), QString(), QString(), &error)
                     .isEmpty());
        QCOMPARE(panel->coverage().measured, 1);
        QCOMPARE(panel->coverage().passing, 0);
        QCOMPARE(panel->coverage().failing, 1);
        QCOMPARE(tree->topLevelItem(0)->text(4), QStringLiteral("4.5 V"));
        QCOMPARE(tree->topLevelItem(0)->text(5), QStringLiteral("Fail"));

        // Both readings stay in the history, newest first, each with the reason
        // for its verdict.
        auto *history = panel->findChild<QTreeWidget *>(QStringLiteral("testpoints.measurements"));
        QVERIFY(history != nullptr);
        QCOMPARE(history->topLevelItemCount(), 2);
        QCOMPARE(history->topLevelItem(0)->text(1), QStringLiteral("4.5 V"));
        QCOMPARE(history->topLevelItem(0)->text(2), QStringLiteral("Fail"));
        QVERIFY2(history->topLevelItem(0)->text(3).contains(QStringLiteral("outside the limits")),
                 qPrintable(history->topLevelItem(0)->text(3)));
        QCOMPARE(history->topLevelItem(1)->text(1), QStringLiteral("5.1 V"));
        QCOMPARE(history->topLevelItem(1)->text(2), QStringLiteral("Pass"));
        QVERIFY2(history->topLevelItem(1)->text(3).contains(QStringLiteral("inside the limits")),
                 qPrintable(history->topLevelItem(1)->text(3)));

        // The filter can show only what is broken.
        auto *filter = panel->findChild<QComboBox *>();
        QVERIFY(filter != nullptr);
        filter->setCurrentIndex(2);   // "Failing"
        QCOMPARE(panel->visibleCount(), 1);
        filter->setCurrentIndex(3);   // "Passing"
        QCOMPARE(panel->visibleCount(), 0);
        filter->setCurrentIndex(0);   // "All"
        QCOMPARE(panel->visibleCount(), 1);

        QVERIFY(project->close());
    }

    void aReadingWithoutAnExpectationIsNotJudged()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::TestPointPanel *panel = m_window->testPointPanel();

        eda::TestPoint point;
        point.identifier = QStringLiteral("TP1");
        point.category = QStringLiteral("resistance");
        point.unit = QStringLiteral("ohm");
        // No expected value: nothing to compare against.
        QString error;
        const QString uid = panel->addTestPoint(point, &error);
        QVERIFY2(!uid.isEmpty(), qPrintable(error));

        QVERIFY(!panel->recordMeasurement(uid, 4700.0, QStringLiteral("ohm"), QString(),
                                          QString(), QString(), &error).isEmpty());

        const eda::Measurement last = eda::latestMeasurement(uid, project->data().measurements);
        QCOMPARE(eda::toString(last.status),
                 eda::toString(eda::MeasurementStatus::Unknown));
        QVERIFY2(eda::measurementExplanation(last).contains(QStringLiteral("cannot pass or fail")),
                 qPrintable(eda::measurementExplanation(last)));

        // It is measured, but it is neither right nor wrong.
        const eda::TestCoverage coverage = panel->coverage();
        QCOMPARE(coverage.measured, 1);
        QCOMPARE(coverage.passing, 0);
        QCOMPARE(coverage.failing, 0);
        QCOMPARE(coverage.unknown, 1);

        auto *tree = panel->findChild<QTreeWidget *>(QStringLiteral("testpoints.tree"));
        QCOMPARE(tree->topLevelItem(0)->text(5), QStringLiteral("Unknown"));

        // A reading in a unit that means something else is not compared either
        // -- and it says so, instead of quietly calling it a failure.
        eda::TestPoint resistance = point;
        resistance.identifier = QStringLiteral("R1.1");
        resistance.category = QStringLiteral("resistance");
        resistance.unit = QStringLiteral("ohm");
        resistance.expectedValue = 4700.0;
        resistance.hasExpectedValue = true;
        const QString resistanceUid = panel->addTestPoint(resistance, &error);
        QVERIFY2(!resistanceUid.isEmpty(), qPrintable(error));
        QVERIFY(!panel->recordMeasurement(resistanceUid, 0.1, QStringLiteral("A"), QString(),
                                          QString(), QString(), &error).isEmpty());
        const eda::Measurement confusing =
            eda::latestMeasurement(resistanceUid, project->data().measurements);
        QCOMPARE(eda::toString(confusing.status),
                 eda::toString(eda::MeasurementStatus::Unknown));
        QVERIFY2(eda::measurementExplanation(confusing).contains(QStringLiteral("not compared")),
                 qPrintable(eda::measurementExplanation(confusing)));
        QCOMPARE(panel->visibleCount(), 2);

        QVERIFY(project->close());
    }

    void testPointsAreStoredWithTheProject()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::TestPointPanel *panel = m_window->testPointPanel();

        eda::TestPoint point;
        point.identifier = QStringLiteral("VCC");
        point.expectedValue = 5.0;
        point.hasExpectedValue = true;
        point.tolerance = 5.0;
        point.hasTolerance = true;
        point.toleranceIsPercent = true;
        point.unit = QStringLiteral("V");
        QString error;
        const QString uid = panel->addTestPoint(point, &error);
        QVERIFY(!uid.isEmpty());
        QVERIFY(!panel->recordMeasurement(uid, 5.05, QStringLiteral("V"), QString(), QString(),
                                          QString(), &error).isEmpty());

        const QString path = eda::TestEnvironment::root() + QStringLiteral("/testpoints.pwb");
        QVERIFY2(project->saveAs(path, &error), qPrintable(error));
        const int testPoints = project->data().testPoints.size();
        const int measurements = project->data().measurements.size();
        QCOMPARE(testPoints, 1);
        QCOMPARE(measurements, 1);

        QVERIFY(project->close());
        QVERIFY2(project->open(path, &error), qPrintable(error));
        QCOMPARE(project->data().testPoints.size(), testPoints);
        QCOMPARE(project->data().measurements.size(), measurements);

        // The panel sees them again, and the percent tolerance came back too.
        QCOMPARE(panel->visibleCount(), 1);
        const eda::Measurement last =
            eda::latestMeasurement(uid, project->data().measurements);
        QCOMPARE(eda::toString(last.status), eda::toString(eda::MeasurementStatus::Pass));
        QCOMPARE(last.tolerance, 0.25);   // 5 % of 5 V, remembered in volts

        // A test point can be taken away; its readings are kept.
        QVERIFY2(panel->removeTestPoint(uid, &error), qPrintable(error));
        QCOMPARE(panel->visibleCount(), 0);
        QCOMPARE(project->data().testPoints.size(), 0);
        QCOMPARE(project->data().measurements.size(), measurements);

        QVERIFY(project->close());
    }

    void theDiagnosticsPanelWeighsWhatWasRecorded()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::DiagnosticsPanel *panel = m_window->diagnosticsPanel();
        QVERIFY(panel != nullptr);
        QCOMPARE(panel->visibleCount(), 0);
        QVERIFY(panel->activeSessionUid().isEmpty());

        QString error;
        const QString session =
            panel->startSession(QStringLiteral("board is dead"),
                                QStringList{ QStringLiteral("no 5 V") }, &error);
        QVERIFY2(!session.isEmpty(), qPrintable(error));
        QCOMPARE(panel->activeSessionUid(), session);
        QVERIFY2(panel->statusText().contains(QStringLiteral("board is dead")),
                 qPrintable(panel->statusText()));

        const QString componentUid =
            project->primaryBoard()->componentByReference(QStringLiteral("C1"))->uid;
        const QString uid = panel->addHypothesis(QStringLiteral("the regulator is dead"),
                                                QStringList{ componentUid }, &error);
        QVERIFY2(!uid.isEmpty(), qPrintable(error));
        QCOMPARE(panel->visibleCount(), 1);

        // Nothing was put forward yet: it stays an idea and scores zero.
        QCOMPARE(panel->scoreOf(uid).score, 0.0);
        QCOMPARE(panel->scoreOf(uid).supporting, 0);
        auto *tree = panel->findChild<QTreeWidget *>(QStringLiteral("diagnostics.tree"));
        QVERIFY(tree != nullptr);
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("the regulator is dead"));
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("Possible"));
        QCOMPARE(tree->topLevelItem(0)->text(3), QStringLiteral("0.00"));   // the score
        QCOMPARE(tree->topLevelItem(0)->text(6), QStringLiteral("C1"));     // the components

        eda::Evidence reading;
        reading.hypothesisUid = uid;
        reading.kind = eda::EvidenceKind::Measurement;
        reading.direction = eda::EvidenceDirection::Supports;
        reading.statement = QStringLiteral("VCC reads 0.02 V");
        reading.provenance.status = eda::VerificationStatus::ExperimentallyVerified;
        reading.provenance.valueSource = eda::ValueSource::Measured;
        QVERIFY2(!panel->addEvidence(reading, &error).isEmpty(), qPrintable(error));

        QCOMPARE(panel->evidenceCount(), 1);
        QCOMPARE(panel->scoreOf(uid).supporting, 1);
        QVERIFY2(panel->scoreOf(uid).score > 0.0,
                 qPrintable(QString::number(panel->scoreOf(uid).score)));
        QVERIFY2(panel->summaryText().contains(QStringLiteral("1 with evidence")),
                 qPrintable(panel->summaryText()));
        // The reading is filed under the session that is open.
        QCOMPARE(project->data().evidence.first().sessionId, session);

        // One reading is not a confirmation, however clear it looks.
        QCOMPARE(tree->topLevelItem(0)->text(2), QStringLiteral("Plausible"));

        eda::Evidence observation = reading;
        observation.kind = eda::EvidenceKind::Observation;
        observation.statement = QStringLiteral("the regulator smells burnt");
        observation.provenance.status = eda::VerificationStatus::UserVerified;
        observation.provenance.valueSource = eda::ValueSource::ManualEntry;
        QVERIFY(!panel->addEvidence(observation, &error).isEmpty());
        QCOMPARE(tree->topLevelItem(0)->text(2),
                 QStringLiteral("Confirmed by measurement"));

        // The recorded status is the user's word, and it is kept apart from
        // what the evidence says.
        auto *status = panel->findChild<QComboBox *>(QStringLiteral("diagnostics.status"));
        QVERIFY(status != nullptr);
        status->setCurrentIndex(status->findData(
            eda::toString(eda::HypothesisStatus::ConfirmedByRepair)));
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("Confirmed by repair"));
        QCOMPARE(eda::toString(project->data().hypotheses.first().status),
                 eda::toString(eda::HypothesisStatus::ConfirmedByRepair));

        QVERIFY2(panel->closeSession(QStringLiteral("replaced the regulator"), &error),
                 qPrintable(error));
        QVERIFY(panel->activeSessionUid().isEmpty());

        QVERIFY2(panel->removeHypothesis(uid, &error), qPrintable(error));
        QCOMPARE(panel->visibleCount(), 0);
        QVERIFY(project->close());
    }

    void whatTheDataSuggestsCanBecomeAnHypothesis()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::TestPointPanel *testPoints = m_window->testPointPanel();
        eda::DiagnosticsPanel *panel = m_window->diagnosticsPanel();

        // With nothing measured there is nothing to point at.
        QCOMPARE(panel->candidateCount(), 0);

        eda::TestPoint point;
        point.identifier = QStringLiteral("VCC");
        point.componentUid = project->primaryBoard()->componentByReference(QStringLiteral("C1"))->uid;
        point.netName = QStringLiteral("VCC");
        point.category = QStringLiteral("voltage");
        point.expectedValue = 5.0;
        point.hasExpectedValue = true;
        point.tolerance = 0.25;
        point.hasTolerance = true;
        point.unit = QStringLiteral("V");
        QString error;
        const QString pointUid = testPoints->addTestPoint(point, &error);
        QVERIFY2(!pointUid.isEmpty(), qPrintable(error));
        QVERIFY(!testPoints->recordMeasurement(pointUid, 4.0, QStringLiteral("V"),
                                               QString(), QString(), QString(), &error)
                     .isEmpty());

        // The failed reading names the net and the parts on it.
        QVERIFY2(panel->candidateCount() > 0, "a reading out of range is a place to look");
        const QStringList uids = panel->acceptCandidates({ 0 }, &error);
        QVERIFY2(!uids.isEmpty(), qPrintable(error));
        QCOMPARE(panel->visibleCount(), 1);

        auto *tree = panel->findChild<QTreeWidget *>(QStringLiteral("diagnostics.tree"));
        QCOMPARE(tree->topLevelItem(0)->text(0), QStringLiteral("the fault is on net VCC"));
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("Possible"));
        QVERIFY2(tree->topLevelItem(0)->text(6).contains(QStringLiteral("C1")),
                 qPrintable(tree->topLevelItem(0)->text(6)));
        QVERIFY2(project->data().hypotheses.first().notes.contains(
                     QStringLiteral("suggested from the data")),
                 qPrintable(project->data().hypotheses.first().notes));

        // A second test point on the same net was never measured: that is where
        // to put the probe next, and the panel says why.
        eda::TestPoint second = point;
        second.identifier = QStringLiteral("VCC2");
        QVERIFY2(!testPoints->addTestPoint(second, &error).isEmpty(), qPrintable(error));
        QVERIFY2(panel->suggestionCount() > 0, "there is always a next step");
        auto *next = panel->findChild<QTreeWidget *>(QStringLiteral("diagnostics.next"));
        QVERIFY(next != nullptr);
        QCOMPARE(next->topLevelItem(0)->text(0), QStringLiteral("VCC2"));
        QVERIFY2(next->topLevelItem(0)->text(2).contains(QStringLiteral("net VCC")),
                 qPrintable(next->topLevelItem(0)->text(2)));

        QVERIFY(project->close());
    }

    void hypothesesAndEvidenceSurviveTheProject()
    {
        eda::Project *project = projectWithImportedBoard();
        QVERIFY(project != nullptr);
        eda::DiagnosticsPanel *panel = m_window->diagnosticsPanel();

        QString error;
        QVERIFY(!panel->startSession(QStringLiteral("unstable rail"), {}, &error).isEmpty());
        const QString uid = panel->addHypothesis(QStringLiteral("C1 is dried out"),
                                                QStringList{ QStringLiteral("comp-x") }, &error);
        QVERIFY(!uid.isEmpty());
        eda::Evidence reading;
        reading.hypothesisUid = uid;
        reading.kind = eda::EvidenceKind::Measurement;
        reading.direction = eda::EvidenceDirection::Supports;
        reading.statement = QStringLiteral("C1 reads 12 nF");
        reading.provenance.status = eda::VerificationStatus::ExperimentallyVerified;
        reading.provenance.valueSource = eda::ValueSource::Measured;
        QVERIFY(!panel->addEvidence(reading, &error).isEmpty());

        const QString path = eda::TestEnvironment::root() + QStringLiteral("/diagnostics.pwb");
        QVERIFY2(project->saveAs(path, &error), qPrintable(error));
        QVERIFY(project->close());
        QVERIFY2(project->open(path, &error), qPrintable(error));

        QCOMPARE(panel->visibleCount(), 1);
        QCOMPARE(project->data().evidence.size(), 1);
        QVERIFY(!panel->activeSessionUid().isEmpty());

        panel->selectHypothesis(uid);
        QCOMPARE(panel->evidenceCount(), 1);
        QCOMPARE(panel->scoreOf(uid).supporting, 1);
        QVERIFY2(panel->summaryText().contains(QStringLiteral("1 with evidence")),
                 qPrintable(panel->summaryText()));

        QVERIFY(project->close());
    }

    void statusBarShowsProjectAndModelState()
    {
        QVERIFY(m_window->statusBar() != nullptr);
        QVERIFY(!m_window->statusBar()->findChildren<QLabel *>().isEmpty());
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
