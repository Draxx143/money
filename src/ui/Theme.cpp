#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QWidget>

namespace eda {

ThemeColors themeColors(const QColor &accent, const QColor &accentSecondary)
{
    ThemeColors colors;
    if (accent.isValid())
        colors.accent = accent;
    if (accentSecondary.isValid())
        colors.accentSecondary = accentSecondary;
    colors.highlight = QColor(colors.accent.red(), colors.accent.green(), colors.accent.blue(), 90);
    return colors;
}

namespace {

QPalette paletteFrom(const ThemeColors &colors)
{
    QPalette palette;

    palette.setColor(QPalette::Window, colors.background);
    palette.setColor(QPalette::WindowText, colors.text);
    palette.setColor(QPalette::Base, colors.card);
    palette.setColor(QPalette::AlternateBase, colors.panel);
    palette.setColor(QPalette::ToolTipBase, colors.panel);
    palette.setColor(QPalette::ToolTipText, colors.text);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::Button, colors.panel);
    palette.setColor(QPalette::ButtonText, colors.text);
    palette.setColor(QPalette::BrightText, colors.warning);
    palette.setColor(QPalette::Link, colors.accentSecondary);
    palette.setColor(QPalette::Highlight, colors.accent);
    palette.setColor(QPalette::HighlightedText, colors.background);

    palette.setColor(QPalette::Light, colors.border);
    palette.setColor(QPalette::Midlight, colors.border);
    palette.setColor(QPalette::Mid, colors.border);
    palette.setColor(QPalette::Dark, colors.background);
    palette.setColor(QPalette::Shadow, colors.background);

    palette.setColor(QPalette::Disabled, QPalette::WindowText, colors.textSecondary);
    palette.setColor(QPalette::Disabled, QPalette::Text, colors.textSecondary);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, colors.textSecondary);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, colors.border);

    palette.setColor(QPalette::PlaceholderText, colors.textSecondary);
    return palette;
}

} // namespace

QString styleSheet(const ThemeColors &colors)
{
    const QString bg = colors.background.name();
    const QString panel = colors.panel.name();
    const QString card = colors.card.name();
    const QString border = colors.border.name();
    const QString accent = colors.accent.name();
    const QString text = colors.text.name();
    const QString textSecondary = colors.textSecondary.name();

    return QStringLiteral(R"(
QWidget { color: %1; }
QMainWindow, QDialog, QDockWidget, QMenuBar { background: %2; }
QMenuBar::item:selected { background: %3; }
QMenu { background: %3; border: 1px solid %4; }
QMenu::item:selected { background: %5; color: %2; }
QToolBar { background: %2; border: 0; border-bottom: 1px solid %4; spacing: 3px; padding: 2px; }
QToolBar::separator { background: %4; width: 1px; margin: 4px; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 3px 6px; }
QToolButton:hover { background: %3; border-color: %4; }
QToolButton:pressed { background: %5; color: %2; }
QToolButton:checked { background: %3; border-color: %5; }
QStatusBar { background: %2; border-top: 1px solid %4; }
QStatusBar::item { border: 0; }
QDockWidget { titlebar-close-icon: none; }
QDockWidget::title { background: %2; border-bottom: 1px solid %4; padding: 4px 6px; text-align: left; }
QDockWidget::float-button, QDockWidget::close-button { background: transparent; }
QTabWidget::pane { border: 1px solid %4; background: %2; }
QTabBar::tab { background: %2; border: 1px solid %4; padding: 4px 10px; margin-right: 2px; }
QTabBar::tab:selected { background: %3; border-bottom-color: %5; }
QHeaderView::section { background: %3; color: %6; border: 0; border-right: 1px solid %4;
                       border-bottom: 1px solid %4; padding: 4px 6px; }
QTableView, QTreeView, QListView, QTextEdit, QPlainTextEdit, QLineEdit, QSpinBox, QDoubleSpinBox,
QComboBox, QListWidget {
    background: %3; border: 1px solid %4; selection-background-color: %5;
    selection-color: %2; alternate-background-color: %2;
}
QTableView::item:hover, QTreeView::item:hover { background: %2; }
QPushButton { background: %3; border: 1px solid %4; border-radius: 4px; padding: 4px 12px; }
QPushButton:hover { border-color: %5; }
QPushButton:pressed { background: %5; color: %2; }
QPushButton:default { border-color: %5; }
QPushButton:disabled { color: %6; }
QComboBox QAbstractItemView, QLineEdit, QSpinBox, QDoubleSpinBox { selection-background-color: %5; }
QGroupBox { border: 1px solid %4; border-radius: 4px; margin-top: 12px; padding-top: 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: %6; }
QScrollBar:vertical { background: %2; width: 12px; margin: 0; }
QScrollBar:horizontal { background: %2; height: 12px; margin: 0; }
QScrollBar::handle { background: %4; border-radius: 5px; min-height: 24px; min-width: 24px; }
QScrollBar::handle:hover { background: %5; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QProgressBar { background: %3; border: 1px solid %4; border-radius: 3px; text-align: center; }
QProgressBar::chunk { background: %5; }
QSplitter::handle { background: %4; }
QToolTip { background: %3; color: %1; border: 1px solid %4; }
QFrame[frameShape="4"], QFrame[frameShape="5"] { color: %4; }
)")
        .arg(text, bg, card, border, accent, textSecondary);
}

void applyTheme(QApplication *application, const ThemeColors &colors)
{
    if (!application)
        return;
    application->setPalette(paletteFrom(colors));
    application->setStyleSheet(styleSheet(colors));
}

void applyTheme(QWidget *widget, const ThemeColors &colors)
{
    if (!widget)
        return;
    widget->setPalette(paletteFrom(colors));
    widget->setStyleSheet(styleSheet(colors));
}

} // namespace eda
