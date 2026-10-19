// UI tests for the main window workbench.
//
// The tests drive the real widgets with the offscreen platform plugin, so no X
// server is needed. Modal dialogs are avoided on purpose: the tests use the same
// entry points the menu actions use, but with the data supplied directly.
#include <QtTest>

#include "../../src/application/AppIdentity.h"
#include "../../src/application/Application.h"
#include "../../src/core/Project.h"
#include "../../src/ui/BoardView.h"
#include "../../src/ui/ComponentExplorer.h"
#include "../../src/ui/MainWindow.h"
#include "../../src/ui/Theme.h"
#include "../TestEnvironment.h"

#include <QApplication>
#include <QAction>
#include <QDockWidget>
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

    void statusBarShowsProjectAndModelState()
    {
        QVERIFY(m_window->statusBar() != nullptr);
        QVERIFY(!m_window->statusBar()->findChildren<QLabel *>().isEmpty());
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
