#include "LogPanel.h"

#include "../core/Logging.h"
#include "../core/Paths.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QTextEdit>
#include <QTextStream>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace eda {

LogPanel::LogPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    m_levelFilter = new QComboBox(this);
    m_levelFilter->addItem(tr("Debug and above"), static_cast<int>(Logger::Debug));
    m_levelFilter->addItem(tr("Info and above"), static_cast<int>(Logger::Info));
    m_levelFilter->addItem(tr("Warnings and above"), static_cast<int>(Logger::Warning));
    m_levelFilter->addItem(tr("Errors only"), static_cast<int>(Logger::Error));
    m_levelFilter->setCurrentIndex(1);
    m_levelFilter->setToolTip(tr("Only messages of this level or higher are shown."));
    toolbar->addWidget(m_levelFilter);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Filter messages..."));
    m_search->setClearButtonEnabled(true);
    toolbar->addWidget(m_search, 1);

    m_autoscroll = new QCheckBox(tr("Auto scroll"), this);
    m_autoscroll->setChecked(true);
    toolbar->addWidget(m_autoscroll);

    auto *saveButton = new QToolButton(this);
    saveButton->setText(tr("Save..."));
    saveButton->setToolTip(tr("Write the visible log lines to a text file."));
    connect(saveButton, &QToolButton::clicked, this, &LogPanel::saveVisibleToFile);
    toolbar->addWidget(saveButton);

    auto *folderButton = new QToolButton(this);
    folderButton->setText(tr("Log folder"));
    folderButton->setToolTip(tr("Open the directory that holds the log files."));
    connect(folderButton, &QToolButton::clicked, this, &LogPanel::openLogDirectory);
    toolbar->addWidget(folderButton);

    auto *clearButton = new QToolButton(this);
    clearButton->setText(tr("Clear"));
    clearButton->setToolTip(tr("Clear the log view. The log file is not affected."));
    connect(clearButton, &QToolButton::clicked, this, &LogPanel::clear);
    toolbar->addWidget(clearButton);

    layout->addLayout(toolbar);

    m_view = new QTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QTextEdit::NoWrap);
    m_view->setUndoRedoEnabled(false);
    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(9);
    m_view->setFont(font);
    layout->addWidget(m_view, 1);

    connect(&Logger::instance(), &Logger::entryAdded, this,
            [this](const Logger::Entry &entry) {
                const QString line = QStringLiteral("%1 [%2] %3: %4")
                                         .arg(entry.timestamp.toString(Qt::ISODateWithMs),
                                              logLevelName(entry.level), entry.category,
                                              entry.message);
                appendEntry(line, static_cast<int>(entry.level));
            },
            Qt::QueuedConnection);

    connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LogPanel::onLevelFilterChanged);
    connect(m_search, &QLineEdit::textChanged, this, &LogPanel::onSearchChanged);

    populateFromHistory();
}

void LogPanel::populateFromHistory()
{
    const QVector<Logger::Entry> history = Logger::instance().history(500);
    for (const Logger::Entry &entry : history) {
        const QString line = QStringLiteral("%1 [%2] %3: %4")
                                 .arg(entry.timestamp.toString(Qt::ISODateWithMs),
                                      logLevelName(entry.level), entry.category, entry.message);
        appendEntry(line, static_cast<int>(entry.level));
    }
}

QString LogPanel::levelName(int level) const
{
    return logLevelName(static_cast<Logger::Level>(level));
}

QString LogPanel::colorForLevel(int level) const
{
    switch (static_cast<Logger::Level>(level)) {
    case Logger::Debug: return QStringLiteral("#8196a8");
    case Logger::Info: return QStringLiteral("#F1F5F9");
    case Logger::Warning: return QStringLiteral("#D29922");
    case Logger::Error: return QStringLiteral("#F85149");
    case Logger::Critical: return QStringLiteral("#FF7B72");
    }
    return QStringLiteral("#F1F5F9");
}

void LogPanel::appendEntry(const QString &line, int level)
{
    m_entries.append({ line, level });
    ++m_entryCount;
    if (m_entries.size() > m_maxEntries)
        m_entries.removeFirst();

    const int minimum = m_levelFilter->currentData().toInt();
    const QString needle = m_search->text().trimmed();
    if (level < minimum)
        return;
    if (!needle.isEmpty() && !line.contains(needle, Qt::CaseInsensitive))
        return;

    const QString html = QStringLiteral("<span style=\"color:%1\">%2</span>")
                             .arg(colorForLevel(level), line.toHtmlEscaped());
    m_view->append(html);
    if (m_autoscroll->isChecked())
        m_view->moveCursor(QTextCursor::End);
}

void LogPanel::refresh()
{
    m_view->clear();
    const int minimum = m_levelFilter->currentData().toInt();
    const QString needle = m_search->text().trimmed();
    for (const StoredEntry &entry : m_entries) {
        if (entry.level < minimum)
            continue;
        if (!needle.isEmpty() && !entry.text.contains(needle, Qt::CaseInsensitive))
            continue;
        const QString html = QStringLiteral("<span style=\"color:%1\">%2</span>")
                                 .arg(colorForLevel(entry.level), entry.text.toHtmlEscaped());
        m_view->append(html);
    }
    m_view->moveCursor(QTextCursor::End);
}

void LogPanel::onLevelFilterChanged() { refresh(); }

void LogPanel::onSearchChanged() { refresh(); }

void LogPanel::clear()
{
    m_view->clear();
    m_entries.clear();
    m_entryCount = 0;
}

void LogPanel::saveVisibleToFile()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("Save log"), QDir::home().absoluteFilePath(QStringLiteral("log.txt")),
        tr("Text files (*.txt);;All files (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save log"),
                             tr("The file cannot be written: %1").arg(file.errorString()));
        return;
    }
    QTextStream out(&file);
    const int minimum = m_levelFilter->currentData().toInt();
    const QString needle = m_search->text().trimmed();
    for (const StoredEntry &entry : m_entries) {
        if (entry.level < minimum)
            continue;
        if (!needle.isEmpty() && !entry.text.contains(needle, Qt::CaseInsensitive))
            continue;
        out << entry.text << '\n';
    }
}

void LogPanel::openLogDirectory()
{
    const QString dir = Logger::instance().logDirectory();
    if (!QFileInfo::exists(dir))
        QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

} // namespace eda
