// Project document: create / open / save / close, dirty tracking and crash
// recovery.
//
// A project is a single JSON document (extension .pwb) that holds everything the
// unified model describes: boards, components, nets, test points, measurements,
// hypotheses, sessions and the documents that were imported. Imported source
// files are referenced by path, not copied, and an import records the file it
// came from so nothing is silently lost.
#pragma once

#include "ProjectModel.h"

#include <QObject>
#include <QString>

class QTimer;

namespace eda {

class Project : public QObject
{
    Q_OBJECT
public:
    explicit Project(QObject *parent = nullptr);
    ~Project() override;

    static QString fileFilter();
    static QString defaultExtension();
    static int schemaVersion();

    bool isOpen() const { return m_open; }
    bool isDirty() const { return m_dirty; }
    QString filePath() const { return m_filePath; }
    QString displayName() const;
    QString directory() const;

    ProjectData &data() { return m_data; }
    const ProjectData &data() const { return m_data; }

    Board *primaryBoard();
    const Board *primaryBoard() const;

    bool create(const QString &name, const QString &directory, QString *error = nullptr);
    bool open(const QString &path, QString *error = nullptr);
    bool save(QString *error = nullptr);
    bool saveAs(const QString &path, QString *error = nullptr);
    bool close(QString *error = nullptr);

    void markDirty();

    // Crash recovery: a copy of the unsaved project, written to the cache
    // directory. Returns true when a usable recovery file was found.
    bool hasRecoveryData() const;
    bool restoreFromRecovery(QString *error = nullptr);
    void discardRecoveryData();
    QString recoveryPath() const;

    void setAutosaveIntervalSeconds(int seconds);
    int autosaveIntervalSeconds() const { return m_autosaveSeconds; }
    QDateTime lastSaved() const { return m_lastSaved; }

signals:
    void opened(const QString &path);
    void saved(const QString &path);
    void closed();
    void dirtyChanged(bool dirty);
    void dataChanged();
    void errorOccurred(const QString &message);

private:
    bool writeTo(const QString &path, QString *error);
    bool readFrom(const QString &path, QString *error);
    void setDirty(bool dirty);
    void scheduleAutosave();

    ProjectData m_data;
    QString m_filePath;
    bool m_open = false;
    bool m_dirty = false;
    QDateTime m_lastSaved;
    QTimer *m_autosaveTimer = nullptr;
    int m_autosaveSeconds = 120;
    bool m_restoring = false;
};

} // namespace eda
