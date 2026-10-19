#include "Settings.h"

#include "core/Paths.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace eda {

Settings::Settings(QObject *parent) : QObject(parent) {}

void Settings::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

bool Settings::load(QString *error)
{
    m_values.clear();
    m_removeKeys.clear();
    m_dirty = false;

    const QString file = m_fileName.isEmpty() ? Paths::settingsFile() : m_fileName;
    if (!QFileInfo::exists(file))
        return true; // first start: defaults are used

    QSettings settings(file, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = tr("Cannot read the configuration file %1.").arg(file);
        return false;
    }
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys)
        m_values.insert(key, settings.value(key));
    return true;
}

bool Settings::save(QString *error)
{
    const QString file = m_fileName.isEmpty() ? Paths::settingsFile() : m_fileName;
    QDir().mkpath(QFileInfo(file).absolutePath());

    QSettings settings(file, QSettings::IniFormat);
    for (const QString &key : m_removeKeys)
        settings.remove(key);
    m_removeKeys.clear();

    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        settings.setValue(it.key(), it.value());
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        if (error)
            *error = tr("Cannot write the configuration file %1.").arg(file);
        return false;
    }
    m_dirty = false;
    emit saved();
    return true;
}

bool Settings::isWritable() const
{
    const QString file = m_fileName.isEmpty() ? Paths::settingsFile() : m_fileName;
    QFileInfo info(file);
    if (info.exists())
        return info.isWritable();
    // The directory does not exist yet (first start): walk up to the closest
    // ancestor that does and ask whether a new directory can be made there.
    // (QDir::cdUp() cannot be used for this: it also requires the directory to
    // exist, which is exactly what is not known here.)
    QFileInfo dir(info.absolutePath());
    while (!dir.exists()) {
        const QString parent = dir.absolutePath();
        if (parent.isEmpty() || parent == dir.filePath())
            return false;
        dir = QFileInfo(parent);
    }
    return dir.isWritable();
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    auto it = m_values.constFind(key);
    if (it == m_values.cend())
        return defaultValue;
    return *it;
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    auto it = m_values.find(key);
    if (it != m_values.end() && it.value() == value)
        return;
    m_values.insert(key, value);
    m_removeKeys.remove(key);
    m_dirty = true;
}

bool Settings::contains(const QString &key) const { return m_values.contains(key); }

void Settings::remove(const QString &key)
{
    m_values.remove(key);
    m_removeKeys.insert(key);
    m_dirty = true;
}

void Settings::reset()
{
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it)
        m_removeKeys.insert(it.key());
    m_values.clear();
    m_dirty = true;
}

QStringList Settings::recentProjects() const
{
    return value(QStringLiteral("recent/projects")).toStringList();
}

void Settings::addRecentProject(const QString &path, int limit)
{
    QStringList projects = recentProjects();
    projects.removeAll(path);
    projects.prepend(path);
    while (projects.size() > limit)
        projects.removeLast();
    setValue(QStringLiteral("recent/projects"), projects);
}

void Settings::clearRecentProjects() { setValue(QStringLiteral("recent/projects"), QStringList()); }

} // namespace eda
