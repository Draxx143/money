// Structured logging.
//
// The logger keeps a bounded in-memory ring buffer (for the log panel), writes
// every entry to a rotating file under the state directory and emits a signal
// for the UI. Qt's own messages (qDebug/qWarning/qCritical and friends) are
// forwarded through qInstallMessageHandler, so a Qt warning ends up in the same
// place as the application's own messages.
#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

namespace eda {

class Logger : public QObject
{
    Q_OBJECT
public:
    enum Level { Debug = 0, Info, Warning, Error, Critical };
    Q_ENUM(Level)

    struct Entry
    {
        QDateTime timestamp;
        Level level = Info;
        QString category;
        QString message;
    };

    static Logger &instance();

    // Where log files are written. Defaults to Paths::logDir().
    void setLogDirectory(const QString &dir);
    QString logDirectory() const { return m_logDirectory; }
    QString currentLogFile() const { return m_logFile; }

    // Verbosity: entries below this level are dropped.
    void setMinimumLevel(Level level);
    Level minimumLevel() const { return m_minimumLevel; }

    int maxHistoryEntries() const { return m_maxHistory; }
    void setMaxHistoryEntries(int n);

    QVector<Entry> history(int maxCount = -1) const;

public slots:
    void log(Level level, const QString &category, const QString &message);

signals:
    void entryAdded(const eda::Logger::Entry &entry);

private:
    Logger();
    ~Logger() override;

    void openLogFile();
    void rotateIfNeeded();

    QString m_logDirectory;
    QString m_logFile;
    Level m_minimumLevel = Info;
    int m_maxHistory = 5000;
    QVector<Entry> m_history;
    bool m_fileOpen = false;
};

// Convenience helpers.
void logDebug(const QString &category, const QString &message);
void logInfo(const QString &category, const QString &message);
void logWarning(const QString &category, const QString &message);
void logError(const QString &category, const QString &message);

// Installs the Qt message handler that forwards everything to the Logger.
void installQtMessageHandler();

QString logLevelName(Logger::Level level);
Logger::Level logLevelFromName(const QString &name);

} // namespace eda
