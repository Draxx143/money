// Dark industrial theme.
//
// The palette is data, not code: every widget reads the colours from here, so
// the accent colour configured in the settings really does change the whole
// application (and the report output) instead of only one widget.
#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QWidget;

namespace eda {

struct ThemeColors
{
    QColor background = QColor(QStringLiteral("#0B0F14"));
    QColor panel = QColor(QStringLiteral("#121923"));
    QColor card = QColor(QStringLiteral("#18222E"));
    QColor border = QColor(QStringLiteral("#283544"));
    QColor accent = QColor(QStringLiteral("#00D1B2"));
    QColor accentSecondary = QColor(QStringLiteral("#4DA3FF"));
    QColor text = QColor(QStringLiteral("#F1F5F9"));
    QColor textSecondary = QColor(QStringLiteral("#94A3B8"));
    QColor success = QColor(QStringLiteral("#3FB950"));
    QColor warning = QColor(QStringLiteral("#D29922"));
    QColor error = QColor(QStringLiteral("#F85149"));

    // Verified / unverified / ruled out style used by diagnostics.
    QColor highlight = QColor(0x00, 0xD1, 0xB2, 90);
    QColor suspect = QColor(0xD2, 0x99, 0x22, 110);
    QColor confirmedFault = QColor(0xF8, 0x51, 0x49, 120);
    QColor ruledOut = QColor(0x94, 0xA3, 0xB8, 70);
};

// Derives the full palette from the configured accent colours.
ThemeColors themeColors(const QColor &accent, const QColor &accentSecondary);

// Applies the palette and the widget stylesheet to a whole application.
void applyTheme(QApplication *application, const ThemeColors &colors);

// Same styling for a single widget subtree (used by dialogs and by tests that
// create widgets without an application level theme).
void applyTheme(QWidget *widget, const ThemeColors &colors);

// Qt stylesheet built from the palette.
QString styleSheet(const ThemeColors &colors);

} // namespace eda
