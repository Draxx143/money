// Local knowledge database.
//
// Everything the application knows about parts and about how to test them lives
// in one SQLite file inside the user's data directory: component records, and
// facts about them (a pinout, an absolute maximum, a typical failure, a note
// from a datasheet) each carrying its own provenance.
//
// The rules that hold for the rest of the model hold here too:
//
//   * no value is invented. A fact is stored with the source it came from, and
//     a fact without a source is stored as unverified, never as confirmed;
//   * the database is never a substitute for the board file: what the file says
//     stays in the project, and the database is the reusable knowledge around
//     it;
//   * a damaged database is reported, never deleted or silently recreated.
//
// Full text search uses SQLite's FTS5. When a search cannot be expressed as an
// FTS5 query the database falls back to a plain text scan and says so through
// lastSearchMode(), so the user interface never has to pretend a search did
// something it did not.
#pragma once

#include "ProjectModel.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace eda {

// What is known about a part, independent of any board.
struct ComponentRecord
{
    QString uid;                   // ours; empty until the record is stored
    QString mpn;
    QString manufacturer;
    ComponentCategory category = ComponentCategory::Unknown;
    QString description;
    QString package;
    QString datasheetUrl;
    QString datasheetPath;
    QString notes;
    Provenance provenance;
    QDateTime created;
    QDateTime updated;

    bool isEmpty() const { return uid.isEmpty() && mpn.isEmpty(); }
    QString displayName() const;
};

// One piece of knowledge, and where it came from.
struct Fact
{
    qlonglong id = 0;              // 0 until stored
    QString componentUid;          // empty for general knowledge
    QString topic;                 // see knowledgeTopics()
    QString statement;             // the fact itself, in words
    QString value;                 // the value as written, when there is one
    QString unit;                  // "V", "ohm", "degC" -- empty when not a value
    QString conditions;            // "at 25 C", "board unpowered"
    Provenance provenance;
    QDateTime created;

    bool isEmpty() const { return id == 0; }
    // One line for lists: the statement, with the topic in front when there is
    // room for it.
    QString summary() const;
};

// The topics a fact can be filed under. They are suggestions, not a closed
// list: any string is accepted, these are what the dialog offers.
QStringList knowledgeTopics();

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    ~Database() override;

    static int schemaVersion();
    static QString defaultPath();

    // Opens (and creates, when the file does not exist) the database at <path>.
    // Fails with a readable reason when the file is not a database, is damaged,
    // or was written by a newer version of the application; the file is left
    // untouched in that case.
    bool open(const QString &path, QString *error = nullptr);
    bool isOpen() const { return m_open; }
    void close();

    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }
    int version() const { return m_version; }

    bool isEmpty() const;
    int componentCount() const;
    int factCount() const;
    QStringList topics() const;

    // --- component records -------------------------------------------------
    // Stores the record. An existing record with the same part number and
    // manufacturer is updated instead of duplicated. The uid is filled in.
    bool upsertComponent(ComponentRecord *record, QString *error = nullptr);
    QVector<ComponentRecord> findComponents(const QString &text, int limit = 50) const;
    ComponentRecord componentByUid(const QString &uid) const;
    bool removeComponent(const QString &uid, QString *error = nullptr);

    // --- facts -------------------------------------------------------------
    bool addFact(Fact *fact, QString *error = nullptr);
    bool updateFact(const Fact &fact, QString *error = nullptr);
    bool removeFact(qlonglong id, QString *error = nullptr);
    QVector<Fact> factsForComponent(const QString &componentUid, int limit = 200) const;
    // Full text search. Empty query returns the most recent facts.
    QVector<Fact> searchFacts(const QString &query, int limit = 50) const;
    QVector<Fact> recentFacts(int limit = 20) const;
    // How the last search was really executed: "fts5", "like" or "recent".
    QString lastSearchMode() const { return m_lastSearchMode; }

    // --- maintenance -------------------------------------------------------
    bool checkIntegrity(QString *error = nullptr) const;
    bool vacuum(QString *error = nullptr);

    // Turns what the user typed into an FTS5 query. Public so that it can be
    // tested: a stray quote or operator in user input must never reach SQLite.
    static QString toMatchQuery(const QString &userText);

signals:
    void opened(const QString &path);
    void changed();

private:
    Q_DISABLE_COPY(Database)

    bool prepare(QString *error);
    bool createSchema(QString *error);
    bool migrate(int fromVersion, QString *error);
    bool run(const QString &sql, QString *error = nullptr);
    bool runScript(const QStringList &statements, QString *error = nullptr);
    void setError(const QString &message) const { m_lastError = message; }

    QString m_path;
    mutable QString m_lastError;
    QString m_connectionName;
    int m_version = 0;
    bool m_open = false;
    mutable QString m_lastSearchMode;
};

} // namespace eda
