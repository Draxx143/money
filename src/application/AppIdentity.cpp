#include "AppIdentity.h"

#include "Settings.h"

#include <QFileInfo>
#include <QPainter>
#include <QPixmap>

namespace eda {

AppIdentity::AppIdentity(QObject *parent) : QObject(parent) {}

void AppIdentity::setIdentity(const Identity &identity)
{
    if (m_identity.name == identity.name && m_identity.subtitle == identity.subtitle
        && m_identity.organization == identity.organization
        && m_identity.iconPath == identity.iconPath && m_identity.accent == identity.accent
        && m_identity.accentSecondary == identity.accentSecondary
        && m_identity.reportTitle == identity.reportTitle
        && m_identity.reportFooter == identity.reportFooter) {
        return;
    }
    m_identity = identity;
    emit changed();
}

QString AppIdentity::windowTitle(const QString &document) const
{
    if (document.trimmed().isEmpty())
        return m_identity.displayName();
    return document.trimmed() + QStringLiteral(" - ") + m_identity.displayName();
}

QIcon AppIdentity::loadIcon(const QString &path) const
{
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        QIcon custom(path);
        if (!custom.isNull())
            return custom;
    }
    return QIcon();
}

QIcon AppIdentity::icon() const
{
    QIcon custom = loadIcon(m_identity.iconPath);
    if (!custom.isNull())
        return custom;

    // Neutral placeholder: a board outline with two pads drawn in the accent
    // colour. It is generated at runtime so that no artwork has to be shipped.
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_identity.accent);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.setBrush(QColor(0x18, 0x22, 0x2E));
    painter.drawRoundedRect(6, 6, 52, 52, 6, 6);

    painter.setBrush(m_identity.accent);
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 4; ++i) {
        painter.drawEllipse(18, 16 + i * 11, 6, 6);
        painter.drawEllipse(40, 16 + i * 11, 6, 6);
    }
    painter.setBrush(m_identity.accentSecondary);
    painter.drawRect(20, 30, 24, 3);
    painter.end();
    return QIcon(pixmap);
}

void AppIdentity::load(Settings *settings)
{
    Identity id;
    id.name = settings->value(QStringLiteral("identity/name"), id.name).toString();
    id.subtitle = settings->value(QStringLiteral("identity/subtitle"), id.subtitle).toString();
    id.organization = settings->value(QStringLiteral("identity/organization"), id.organization).toString();
    id.iconPath = settings->value(QStringLiteral("identity/iconPath"), id.iconPath).toString();
    id.accent = QColor(settings->value(QStringLiteral("identity/accent"), id.accent.name()).toString());
    id.accentSecondary = QColor(
        settings->value(QStringLiteral("identity/accentSecondary"), id.accentSecondary.name()).toString());
    id.reportTitle = settings->value(QStringLiteral("identity/reportTitle"), id.reportTitle).toString();
    id.reportFooter = settings->value(QStringLiteral("identity/reportFooter"), id.reportFooter).toString();
    if (!id.accent.isValid())
        id.accent = QColor(QStringLiteral("#00D1B2"));
    if (!id.accentSecondary.isValid())
        id.accentSecondary = QColor(QStringLiteral("#4DA3FF"));
    setIdentity(id);
}

void AppIdentity::save(Settings *settings) const
{
    settings->setValue(QStringLiteral("identity/name"), m_identity.name);
    settings->setValue(QStringLiteral("identity/subtitle"), m_identity.subtitle);
    settings->setValue(QStringLiteral("identity/organization"), m_identity.organization);
    settings->setValue(QStringLiteral("identity/iconPath"), m_identity.iconPath);
    settings->setValue(QStringLiteral("identity/accent"), m_identity.accent.name());
    settings->setValue(QStringLiteral("identity/accentSecondary"), m_identity.accentSecondary.name());
    settings->setValue(QStringLiteral("identity/reportTitle"), m_identity.reportTitle);
    settings->setValue(QStringLiteral("identity/reportFooter"), m_identity.reportFooter);
}

} // namespace eda
