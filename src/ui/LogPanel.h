// Bottom dock panel that shows the application log.
//
// Entries are appended as they arrive (the logger runs on any thread, the
// signal crosses to the GUI thread with a queued connection), can be filtered by
// level and searched, and the panel can save the visible entries to a file.
#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTextEdit;

namespace eda {

class LogPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogPanel(QWidget *parent = nullptr);

    int entryCount() const { return m_entryCount; }

public slots:
    void clear();
    void saveVisibleToFile();
    void openLogDirectory();

private slots:
    void appendEntry(const QString &line, int level);
    void refresh();
    void onLevelFilterChanged();
    void onSearchChanged();
    void populateFromHistory();

private:
    QString colorForLevel(int level) const;
    QString levelName(int level) const;

    QTextEdit *m_view = nullptr;
    QComboBox *m_levelFilter = nullptr;
    QLineEdit *m_search = nullptr;
    QCheckBox *m_autoscroll = nullptr;
    int m_entryCount = 0;
    int m_maxEntries = 2000;
    struct StoredEntry
    {
        QString text;
        int level;
    };
    QList<StoredEntry> m_entries;
};

} // namespace eda
