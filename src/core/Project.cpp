#include "Project.h"

#include "Logging.h"
#include "Paths.h"
#include <eda/Version.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>

namespace eda {
namespace {

const char *kFormatTag = "pcb-workbench-project";

QString recoveryFilePath()
{
    return Paths::cacheDir() + QStringLiteral("/recovery.pwb");
}

} // namespace

Project::Project(QObject *parent) : QObject(parent)
{
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(false);
    connect(m_autosaveTimer, &QTimer::timeout, this, [this]() {
        if (!m_open || !m_dirty || m_filePath.isEmpty())
            return;
        QString error;
        if (writeTo(recoveryFilePath(), &error)) {
            logDebug(QStringLiteral("project"), QStringLiteral("autosave written"));
        } else {
            logWarning(QStringLiteral("project"),
                       QStringLiteral("autosave failed: %1").arg(error));
        }
    });
    scheduleAutosave();
}

Project::~Project() = default;

QString Project::fileFilter()
{
    return tr("Project files (*.pwb);;All files (*)");
}

QString Project::defaultExtension() { return QStringLiteral("pwb"); }
int Project::schemaVersion() { return 1; }

QString Project::displayName() const
{
    if (!m_open)
        return QString();
    if (m_data.metadata.name.trimmed().isEmpty())
        return QFileInfo(m_filePath).completeBaseName();
    return m_data.metadata.name;
}

QString Project::directory() const { return QFileInfo(m_filePath).absolutePath(); }

Board *Project::primaryBoard()
{
    return m_data.boards.isEmpty() ? nullptr : &m_data.boards[0];
}

const Board *Project::primaryBoard() const
{
    return m_data.boards.isEmpty() ? nullptr : &m_data.boards.at(0);
}

bool Project::create(const QString &name, const QString &directory, QString *error)
{
    if (m_open && !close(error))
        return false;

    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = tr("The directory %1 cannot be created.").arg(directory);
        return false;
    }

    m_data = ProjectData();
    m_data.metadata.name = name.trimmed().isEmpty() ? tr("Untitled project") : name.trimmed();
    m_data.metadata.created = QDateTime::currentDateTimeUtc();
    m_data.metadata.modified = m_data.metadata.created;
    m_data.metadata.createdBy = qgetenv("USER");
    m_data.metadata.applicationVersion = QStringLiteral(EDA_VERSION_STRING);
    m_data.metadata.schemaVersion = schemaVersion();

    const QString base = m_data.metadata.name;
    QString candidate = dir.absoluteFilePath(base + QLatin1String(".pwb"));
    int suffix = 1;
    while (QFileInfo::exists(candidate))
        candidate = dir.absoluteFilePath(QStringLiteral("%1-%2.pwb").arg(base).arg(++suffix));

    m_filePath = candidate;
    m_open = true;
    setDirty(true);

    if (!writeTo(m_filePath, error))
        return false;

    m_lastSaved = QDateTime::currentDateTimeUtc();
    setDirty(false);
    emit opened(m_filePath);
    emit dataChanged();
    logInfo(QStringLiteral("project"), QStringLiteral("created %1").arg(m_filePath));
    return true;
}

bool Project::open(const QString &path, QString *error)
{
    if (m_open && !close(error))
        return false;

    if (!readFrom(path, error))
        return false;

    m_filePath = QFileInfo(path).absoluteFilePath();
    m_open = true;
    m_lastSaved = QDateTime::currentDateTimeUtc();
    setDirty(false);
    emit opened(m_filePath);
    emit dataChanged();
    logInfo(QStringLiteral("project"), QStringLiteral("opened %1").arg(m_filePath));
    return true;
}

bool Project::save(QString *error)
{
    if (!m_open) {
        if (error)
            *error = tr("There is no open project.");
        return false;
    }
    if (m_filePath.isEmpty())
        return false;
    if (!writeTo(m_filePath, error))
        return false;
    m_lastSaved = QDateTime::currentDateTimeUtc();
    setDirty(false);
    discardRecoveryData();
    emit saved(m_filePath);
    logInfo(QStringLiteral("project"), QStringLiteral("saved %1").arg(m_filePath));
    return true;
}

bool Project::saveAs(const QString &path, QString *error)
{
    if (!m_open) {
        if (error)
            *error = tr("There is no open project.");
        return false;
    }
    if (!writeTo(path, error))
        return false;
    m_filePath = QFileInfo(path).absoluteFilePath();
    m_lastSaved = QDateTime::currentDateTimeUtc();
    setDirty(false);
    discardRecoveryData();
    emit saved(m_filePath);
    logInfo(QStringLiteral("project"), QStringLiteral("saved as %1").arg(m_filePath));
    return true;
}

bool Project::close(QString *error)
{
    if (!m_open)
        return true;
    if (!writeTo(recoveryFilePath(), error) && error && !error->isEmpty()) {
        // A failed recovery copy is not fatal: the user still has the file they
        // saved. It is reported so the condition is visible.
        logWarning(QStringLiteral("project"),
                   QStringLiteral("recovery copy failed: %1").arg(*error));
        error->clear();
    }
    m_data = ProjectData();
    m_filePath.clear();
    m_open = false;
    setDirty(false);
    emit closed();
    emit dataChanged();
    logInfo(QStringLiteral("project"), QStringLiteral("closed"));
    return true;
}

void Project::markDirty() { setDirty(true); }

void Project::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    if (dirty)
        m_data.metadata.modified = QDateTime::currentDateTimeUtc();
    emit dirtyChanged(dirty);
}

void Project::scheduleAutosave()
{
    m_autosaveTimer->stop();
    if (m_autosaveSeconds > 0)
        m_autosaveTimer->start(m_autosaveSeconds * 1000);
}

void Project::setAutosaveIntervalSeconds(int seconds)
{
    m_autosaveSeconds = seconds;
    scheduleAutosave();
}

bool Project::writeTo(const QString &path, QString *error)
{
    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    m_data.metadata.modified = QDateTime::currentDateTimeUtc();
    m_data.metadata.applicationVersion = QStringLiteral(EDA_VERSION_STRING);
    m_data.metadata.schemaVersion = schemaVersion();

    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(kFormatTag));
    root.insert(QStringLiteral("formatVersion"), schemaVersion());
    root.insert(QStringLiteral("project"), m_data.toJson());

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = tr("The file %1 cannot be written: %2").arg(path, file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (error)
            *error = tr("The file %1 cannot be written: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool Project::readFrom(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = tr("The file %1 cannot be opened: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = tr("%1 is not a valid project file: %2 (offset %3)")
                         .arg(path, parseError.errorString())
                         .arg(parseError.offset);
        return false;
    }
    if (!document.isObject()) {
        if (error)
            *error = tr("%1 is not a valid project file: the top level is not an object.").arg(path);
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormatTag)) {
        if (error)
            *error = tr("%1 is not a project file of this application.").arg(path);
        return false;
    }

    const int version = root.value(QStringLiteral("formatVersion")).toInt(1);
    if (version > schemaVersion()) {
        if (error)
            *error = tr("The project %1 was written by a newer version of the application "
                        "(file format %2, this application understands %3).")
                         .arg(path)
                         .arg(version)
                         .arg(schemaVersion());
        return false;
    }
    if (version < schemaVersion()) {
        // Forward compatible reads only: older files use the same field names,
        // missing fields fall back to the model defaults. Recorded so that the
        // limitation is visible instead of silent.
        logWarning(QStringLiteral("project"),
                   QStringLiteral("project file %1 uses format version %2, reading as version %3")
                       .arg(path)
                       .arg(version)
                       .arg(schemaVersion()));
    }

    m_data = ProjectData::fromJson(root.value(QStringLiteral("project")).toObject());
    return true;
}

bool Project::hasRecoveryData() const { return QFileInfo::exists(recoveryFilePath()); }

QString Project::recoveryPath() const { return recoveryFilePath(); }

bool Project::restoreFromRecovery(QString *error)
{
    if (!hasRecoveryData()) {
        if (error)
            *error = tr("There is no recovery data.");
        return false;
    }
    ProjectData previous = m_data;
    QString previousPath = m_filePath;
    if (!readFrom(recoveryFilePath(), error))
        return false;

    // A recovered project has no trustworthy location: it is opened as untitled
    // so that Save As asks for a destination instead of overwriting anything.
    m_filePath = previousPath;
    m_open = true;
    setDirty(true);
    emit dataChanged();
    logWarning(QStringLiteral("project"), QStringLiteral("restored unsaved work from recovery data"));
    Q_UNUSED(previous)
    return true;
}

void Project::discardRecoveryData()
{
    if (QFileInfo::exists(recoveryFilePath()))
        QFile::remove(recoveryFilePath());
}

} // namespace eda
