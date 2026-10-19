// Persistent application configuration.
//
// One class owns every setting; nothing else in the application talks to
// QSettings directly. Values are stored in an INI file under the XDG config
// directory (Paths::settingsFile()) so that the configuration is easy to
// inspect, back up and remove.
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace eda {

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    // File the settings are stored in (empty -> platform default location).
    void setFileName(const QString &fileName);
    QString fileName() const { return m_fileName; }

    bool load(QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool isWritable() const;

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);
    bool contains(const QString &key) const;
    void remove(const QString &key);
    void reset();

    // Recently used projects (most recent first).
    QStringList recentProjects() const;
    void addRecentProject(const QString &path, int limit = 10);
    void clearRecentProjects();

signals:
    // Emitted once after save() when at least one value changed.
    void saved();

private:
    friend class SettingsTestAccess;

    QString m_fileName;
    QHash<QString, QVariant> m_values;
    QHash<QString, QVariant> m_defaults;
    // Keys that must be removed from the file on the next save (a setting that
    // was reset to its default leaves no entry behind).
    QSet<QString> m_removeKeys;
    bool m_dirty = false;
};

} // namespace eda
