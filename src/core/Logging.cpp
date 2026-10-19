#include "Logging.h"

#include "Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

namespace eda {

namespace {
QMutex g_mutex;
QtMessageHandler g_previousHandler = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Logger::Level level = Logger::Debug;
    switch (type) {
    case QtDebugMsg: level = Logger::Debug; break;
    case QtInfoMsg: level = Logger::Info; break;
    case QtWarningMsg: level = Logger::Warning; break;
    case QtCriticalMsg:
    case QtFatalMsg: level = Logger::Critical; break;
    }
    const QString category = context.category ? QString::fromUtf8(context.category)
                                              : QStringLiteral("qt");
    Logger::instance().log(level, category, msg);
    if (type == QtFatalMsg)
        abort();
}
} // namespace

Logger::Logger()
{
    m_logDirectory = Paths::logDir();
    openLogFile();
}

Logger::~Logger() = default;

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::setLogDirectory(const QString &dir)
{
    QMutexLocker locker(&g_mutex);
    if (dir == m_logDirectory)
        return;
    m_logDirectory = dir;
    m_fileOpen = false;
    openLogFile();
}

void Logger::setMinimumLevel(Level level)
{
    m_minimumLevel = level;
}

void Logger::setMaxHistoryEntries(int n)
{
    QMutexLocker locker(&g_mutex);
    m_maxHistory = qMax(100, n);
    if (m_history.size() > m_maxHistory)
        m_history.remove(0, m_history.size() - m_maxHistory);
}

QVector<Logger::Entry> Logger::history(int maxCount) const
{
    QMutexLocker locker(&g_mutex);
    if (maxCount < 0 || m_history.size() <= maxCount)
        return m_history;
    return m_history.mid(m_history.size() - maxCount);
}

void Logger::openLogFile()
{
    QDir().mkpath(m_logDirectory);
    m_logFile = m_logDirectory + QLatin1String("/application.log");
    QFile file(m_logFile);
    m_fileOpen = file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (!m_fileOpen) {
        std::fprintf(stderr, "cannot open log file %s\n", qPrintable(m_logFile));
    }
}

void Logger::rotateIfNeeded()
{
    // Keep five generations of at most 2 MiB each.
    const qint64 limit = 2 * 1024 * 1024;
    QFileInfo info(m_logFile);
    if (!info.exists() || info.size() < limit)
        return;

    for (int generation = 4; generation >= 1; --generation) {
        const QString from = QStringLiteral("%1.%2").arg(m_logFile).arg(generation);
        const QString to = QStringLiteral("%1.%2").arg(m_logFile).arg(generation + 1);
        QFile::remove(to);
        QFile::rename(from, to);
    }
    QFile::rename(m_logFile, m_logFile + QLatin1String(".1"));
    openLogFile();
}

void Logger::log(Level level, const QString &category, const QString &message)
{
    if (level < m_minimumLevel)
        return;

    Entry entry;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    entry.level = level;
    entry.category = category.isEmpty() ? QStringLiteral("app") : category;
    entry.message = message;

    {
        QMutexLocker locker(&g_mutex);
        m_history.append(entry);
        if (m_history.size() > m_maxHistory)
            m_history.remove(0, m_history.size() - m_maxHistory);
    }

    const QString line = QStringLiteral("%1 [%2] %3: %4\n")
                             .arg(entry.timestamp.toString(Qt::ISODateWithMs),
                                  logLevelName(level), entry.category, message);

    {
        QMutexLocker locker(&g_mutex);
        rotateIfNeeded();
        QFile file(m_logFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << line;
        }
    }

    std::fprintf(stderr, "%s", qPrintable(line));
    std::fflush(stderr);

    emit entryAdded(entry);
}

QString logLevelName(Logger::Level level)
{
    switch (level) {
    case Logger::Debug: return QStringLiteral("DBG");
    case Logger::Info: return QStringLiteral("INF");
    case Logger::Warning: return QStringLiteral("WRN");
    case Logger::Error: return QStringLiteral("ERR");
    case Logger::Critical: return QStringLiteral("CRT");
    }
    return QStringLiteral("???");
}

Logger::Level logLevelFromName(const QString &name)
{
    const QString n = name.trimmed().toLower();
    // Long names come from the settings UI, short names from the log file
    // itself; both have to be understood so a user can paste what she reads.
    if (n == QLatin1String("debug") || n == QLatin1String("dbg")) return Logger::Debug;
    if (n == QLatin1String("info") || n == QLatin1String("inf")) return Logger::Info;
    if (n == QLatin1String("warning") || n == QLatin1String("wrn")) return Logger::Warning;
    if (n == QLatin1String("error") || n == QLatin1String("err")) return Logger::Error;
    if (n == QLatin1String("critical") || n == QLatin1String("crt")) return Logger::Critical;
    if (n == QLatin1String("off")) return Logger::Critical;
    return Logger::Info;
}

void logDebug(const QString &category, const QString &message)
{
    Logger::instance().log(Logger::Debug, category, message);
}
void logInfo(const QString &category, const QString &message)
{
    Logger::instance().log(Logger::Info, category, message);
}
void logWarning(const QString &category, const QString &message)
{
    Logger::instance().log(Logger::Warning, category, message);
}
void logError(const QString &category, const QString &message)
{
    Logger::instance().log(Logger::Error, category, message);
}

void installQtMessageHandler()
{
    g_previousHandler = qInstallMessageHandler(messageHandler);
}

} // namespace eda
