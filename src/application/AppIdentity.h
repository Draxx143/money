// Configurable application identity.
//
// The product name is never hardcoded in the user interface: window titles,
// the splash text, report headers and the About dialog all read it from here,
// so changing the name in the settings changes it everywhere (including after a
// restart). The placeholders below are neutral defaults, not a product name.
#pragma once

#include <QColor>
#include <QIcon>
#include <QObject>
#include <QString>

namespace eda {

struct Identity
{
    QString name = QStringLiteral("PCB Workbench");
    QString subtitle = QStringLiteral("PCB analysis and diagnostics workbench");
    QString organization;
    QString iconPath;          // empty -> built-in placeholder icon
    QColor accent = QColor(QStringLiteral("#00D1B2"));
    QColor accentSecondary = QColor(QStringLiteral("#4DA3FF"));

    // Report branding
    QString reportTitle;       // empty -> derived from name
    QString reportFooter;      // empty -> derived from organization/name

    QString displayName() const { return name.trimmed().isEmpty() ? QStringLiteral("PCB Workbench") : name.trimmed(); }
    QString reportTitleOrDerived() const
    {
        return reportTitle.trimmed().isEmpty() ? displayName() + QStringLiteral(" - Diagnostic Report")
                                               : reportTitle.trimmed();
    }
    QString reportFooterOrDerived() const
    {
        if (!reportFooter.trimmed().isEmpty())
            return reportFooter.trimmed();
        if (!organization.trimmed().isEmpty())
            return organization.trimmed();
        return displayName();
    }
};

class AppIdentity : public QObject
{
    Q_OBJECT
public:
    explicit AppIdentity(QObject *parent = nullptr);

    Identity identity() const { return m_identity; }
    void setIdentity(const Identity &identity);

    QString name() const { return m_identity.displayName(); }

    // "<document> - <application name>"; just the application name when there is
    // no document.
    QString windowTitle(const QString &document = QString()) const;

    // Icon shown in the window title bar, the task bar and the About dialog.
    QIcon icon() const;

    void load(class Settings *settings);
    void save(class Settings *settings) const;

signals:
    void changed();

private:
    QIcon loadIcon(const QString &path) const;

    Identity m_identity;
};

} // namespace eda
