#include "Database.h"

#include "Logging.h"
#include "Paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>

#include <algorithm>

namespace eda {
namespace {

// Connection names have to be unique and are removed when the database is
// closed, so that a test can open several databases in one process.
int g_connectionCounter = 0;

// A null QString would be bound as SQL NULL, and every text column here is NOT
// NULL: something that is unknown is stored as an empty string, never as NULL.
QVariant textValue(const QString &value)
{
    return value.isNull() ? QVariant(QLatin1String("")) : QVariant(value);
}

QString isoString(const QDateTime &when)
{
    return when.isValid() ? when.toUTC().toString(Qt::ISODate) : QString();
}

QDateTime fromIso(const QString &text)
{
    return text.isEmpty() ? QDateTime()
                          : QDateTime::fromString(text, Qt::ISODate).toUTC();
}

const char *kSqlCreateSchema[] = {
    "CREATE TABLE meta ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL)",

    "CREATE TABLE components ("
    "  uid TEXT PRIMARY KEY,"
    "  mpn TEXT NOT NULL DEFAULT '',"
    "  manufacturer TEXT NOT NULL DEFAULT '',"
    "  category TEXT NOT NULL DEFAULT '',"
    "  description TEXT NOT NULL DEFAULT '',"
    "  package TEXT NOT NULL DEFAULT '',"
    "  datasheet_url TEXT NOT NULL DEFAULT '',"
    "  datasheet_path TEXT NOT NULL DEFAULT '',"
    "  notes TEXT NOT NULL DEFAULT '',"
    "  source_title TEXT NOT NULL DEFAULT '',"
    "  source_url TEXT NOT NULL DEFAULT '',"
    "  source_path TEXT NOT NULL DEFAULT '',"
    "  source_location TEXT NOT NULL DEFAULT '',"
    "  status TEXT NOT NULL DEFAULT '',"
    "  value_source TEXT NOT NULL DEFAULT '',"
    "  retrieved TEXT NOT NULL DEFAULT '',"
    "  created TEXT NOT NULL DEFAULT '',"
    "  updated TEXT NOT NULL DEFAULT '')",

    "CREATE INDEX components_mpn ON components(mpn COLLATE NOCASE, "
    "manufacturer COLLATE NOCASE)",

    "CREATE TABLE facts ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  component_uid TEXT NOT NULL DEFAULT '',"
    "  topic TEXT NOT NULL DEFAULT '',"
    "  statement TEXT NOT NULL,"
    "  value TEXT NOT NULL DEFAULT '',"
    "  unit TEXT NOT NULL DEFAULT '',"
    "  conditions TEXT NOT NULL DEFAULT '',"
    "  source_title TEXT NOT NULL DEFAULT '',"
    "  source_publisher TEXT NOT NULL DEFAULT '',"
    "  source_url TEXT NOT NULL DEFAULT '',"
    "  source_path TEXT NOT NULL DEFAULT '',"
    "  source_location TEXT NOT NULL DEFAULT '',"
    "  status TEXT NOT NULL DEFAULT '',"
    "  value_source TEXT NOT NULL DEFAULT '',"
    "  retrieved TEXT NOT NULL DEFAULT '',"
    "  notes TEXT NOT NULL DEFAULT '',"
    "  created TEXT NOT NULL DEFAULT '')",

    "CREATE INDEX facts_component ON facts(component_uid)",
    "CREATE INDEX facts_topic ON facts(topic)",

    // The full text index keeps no copy of the text: it points at facts, so
    // there is exactly one place where a statement is stored.
    "CREATE VIRTUAL TABLE facts_fts USING fts5("
    "  statement, topic, source_title,"
    "  content='facts', content_rowid='id')",

    "CREATE TRIGGER facts_ai AFTER INSERT ON facts BEGIN"
    "  INSERT INTO facts_fts(rowid, statement, topic, source_title)"
    "  VALUES (new.id, new.statement, new.topic, new.source_title);"
    "END",

    "CREATE TRIGGER facts_ad AFTER DELETE ON facts BEGIN"
    "  INSERT INTO facts_fts(facts_fts, rowid, statement, topic, source_title)"
    "  VALUES ('delete', old.id, old.statement, old.topic, old.source_title);"
    "END",

    "CREATE TRIGGER facts_au AFTER UPDATE ON facts BEGIN"
    "  INSERT INTO facts_fts(facts_fts, rowid, statement, topic, source_title)"
    "  VALUES ('delete', old.id, old.statement, old.topic, old.source_title);"
    "  INSERT INTO facts_fts(rowid, statement, topic, source_title)"
    "  VALUES (new.id, new.statement, new.topic, new.source_title);"
    "END"
};

QStringList createSchemaStatements()
{
    QStringList statements;
    for (const char *sql : kSqlCreateSchema)
        statements.append(QString::fromLatin1(sql));
    return statements;
}

ComponentRecord componentFromQuery(const QSqlQuery &query)
{
    ComponentRecord record;
    const QSqlRecord fields = query.record();
    auto text = [&](const char *name) {
        const int index = fields.indexOf(QString::fromLatin1(name));
        return index < 0 ? QString() : query.value(index).toString();
    };

    record.uid = text("uid");
    record.mpn = text("mpn");
    record.manufacturer = text("manufacturer");
    record.category = componentCategoryFromString(text("category"));
    record.description = text("description");
    record.package = text("package");
    record.datasheetUrl = text("datasheet_url");
    record.datasheetPath = text("datasheet_path");
    record.notes = text("notes");

    record.provenance.title = text("source_title");
    record.provenance.url = text("source_url");
    record.provenance.localPath = text("source_path");
    record.provenance.location = text("source_location");
    record.provenance.status = verificationStatusFromString(text("status"));
    record.provenance.valueSource = valueSourceFromString(text("value_source"));
    record.provenance.retrieved = fromIso(text("retrieved"));

    record.created = fromIso(text("created"));
    record.updated = fromIso(text("updated"));
    return record;
}

Fact factFromQuery(const QSqlQuery &query)
{
    Fact fact;
    const QSqlRecord fields = query.record();
    auto text = [&](const char *name) {
        const int index = fields.indexOf(QString::fromLatin1(name));
        return index < 0 ? QString() : query.value(index).toString();
    };

    fact.id = text("id").toLongLong();
    if (fact.id == 0 && fields.contains(QStringLiteral("id")))
        fact.id = query.value(fields.indexOf(QStringLiteral("id"))).toLongLong();
    fact.componentUid = text("component_uid");
    fact.topic = text("topic");
    fact.statement = text("statement");
    fact.value = text("value");
    fact.unit = text("unit");
    fact.conditions = text("conditions");

    fact.provenance.title = text("source_title");
    fact.provenance.publisher = text("source_publisher");
    fact.provenance.url = text("source_url");
    fact.provenance.localPath = text("source_path");
    fact.provenance.location = text("source_location");
    fact.provenance.status = verificationStatusFromString(text("status"));
    fact.provenance.valueSource = valueSourceFromString(text("value_source"));
    fact.provenance.retrieved = fromIso(text("retrieved"));
    fact.provenance.notes = text("notes");

    fact.created = fromIso(text("created"));
    return fact;
}

} // namespace

QString ComponentRecord::displayName() const
{
    if (!mpn.isEmpty())
        return manufacturer.isEmpty() ? mpn : QStringLiteral("%1 %2").arg(manufacturer, mpn);
    if (!description.isEmpty())
        return description;
    if (!package.isEmpty())
        return package;
    return uid;
}

QString Fact::summary() const
{
    QString line = statement;
    line.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (line.size() > 160)
        line = line.left(157) + QStringLiteral("...");
    if (!topic.isEmpty())
        line = QStringLiteral("[%1] %2").arg(topic, line);
    return line;
}

QStringList knowledgeTopics()
{
    return { QStringLiteral("pinout"),
             QStringLiteral("absolute-maximum"),
             QStringLiteral("typical-value"),
             QStringLiteral("measurement"),
             QStringLiteral("failure-mode"),
             QStringLiteral("replacement"),
             QStringLiteral("datasheet-note"),
             QStringLiteral("board-note") };
}

Database::Database(QObject *parent) : QObject(parent) {}

Database::~Database() { close(); }

int Database::schemaVersion() { return 1; }

QString Database::defaultPath() { return Paths::databaseFile(); }

bool Database::open(const QString &path, QString *error)
{
    if (m_open)
        close();

    const QFileInfo info(path);
    if (info.exists() && !info.isFile()) {
        setError(QStringLiteral("There is something at %1 that is not a file.").arg(path));
        if (error)
            *error = m_lastError;
        return false;
    }

    QDir directory = info.absoluteDir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("The directory %1 cannot be created.")
                     .arg(directory.absolutePath()));
        if (error)
            *error = m_lastError;
        return false;
    }

    m_connectionName = QStringLiteral("eda-knowledge-%1").arg(++g_connectionCounter);
    QString openError;
    {
        // The connection copy is dropped before the database is ever removed or
        // closed: Qt warns (and stops answering queries) when a connection is
        // removed while a copy of it is still alive.
        QSqlDatabase connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                           m_connectionName);
        connection.setDatabaseName(path);
        if (!connection.open())
            openError = connection.lastError().text();
    }
    if (!openError.isEmpty()) {
        setError(QStringLiteral("The database %1 cannot be opened: %2").arg(path, openError));
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        if (error)
            *error = m_lastError;
        return false;
    }

    m_path = path;

    // A damaged file is reported, never repaired by throwing it away: the
    // knowledge in it may be the only copy.
    QString checkResult;
    bool checkFailed = false;
    {
        QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
        QSqlQuery check(connection);
        if (!check.exec(QStringLiteral("PRAGMA quick_check"))) {
            checkFailed = true;
            checkResult = check.lastError().text();
        } else if (check.next()) {
            checkResult = check.value(0).toString();
            checkFailed = checkResult.compare(QLatin1String("ok"), Qt::CaseInsensitive) != 0;
        }
    }
    if (checkFailed) {
        setError(QStringLiteral("The database %1 is damaged (%2). It was left untouched; move "
                                "it aside to start with an empty one.")
                     .arg(path, checkResult));
        close();
        if (error)
            *error = m_lastError;
        return false;
    }

    if (!prepare(error)) {
        close();
        return false;
    }

    m_open = true;
    logInfo(QStringLiteral("database"), QStringLiteral("knowledge database: %1 (schema %2)")
                                            .arg(path)
                                            .arg(m_version));
    emit opened(path);
    return true;
}

bool Database::prepare(QString *error)
{
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(connection);

    bool hasMeta = false;
    if (query.exec(QStringLiteral("SELECT value FROM meta WHERE key = 'schema_version'"))) {
        if (query.next()) {
            hasMeta = true;
            m_version = query.value(0).toInt();
        }
    }

    if (!hasMeta || m_version == 0) {
        if (!createSchema(error))
            return false;
        m_version = schemaVersion();
        return true;
    }

    if (m_version > schemaVersion()) {
        setError(QStringLiteral("The database %1 was written by a newer version of the "
                                "application (schema %2, this version understands %3).")
                     .arg(m_path)
                     .arg(m_version)
                     .arg(schemaVersion()));
        if (error)
            *error = m_lastError;
        return false;
    }

    return migrate(m_version, error);
}

bool Database::createSchema(QString *error)
{
    if (!runScript(createSchemaStatements(), error))
        return false;

    const QString now = isoString(QDateTime::currentDateTimeUtc());
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("INSERT INTO meta (key, value) VALUES (?, ?)"));
    query.addBindValue(textValue(QStringLiteral("schema_version")));
    query.addBindValue(textValue(QString::number(schemaVersion())));
    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    query.prepare(QStringLiteral("INSERT INTO meta (key, value) VALUES (?, ?)"));
    query.addBindValue(QStringLiteral("created"));
    query.addBindValue(textValue(now));
    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    logInfo(QStringLiteral("database"),
            QStringLiteral("created a new knowledge database at %1").arg(m_path));
    return true;
}

bool Database::migrate(int fromVersion, QString *error)
{
    // Every step from <fromVersion> to schemaVersion() is added here when the
    // schema changes. There is nothing to do for schema 1, which is the first.
    Q_UNUSED(fromVersion)
    Q_UNUSED(error)
    return true;
}

bool Database::run(const QString &sql, QString *error)
{
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(connection);
    if (!query.exec(sql)) {
        setError(QStringLiteral("%1: %2").arg(query.lastError().text(), sql));
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Database::runScript(const QStringList &statements, QString *error)
{
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    if (!connection.transaction()) {
        setError(connection.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    for (const QString &statement : statements) {
        QSqlQuery query(connection);
        if (!query.exec(statement)) {
            const QString message = QStringLiteral("%1: %2").arg(query.lastError().text(),
                                                                 statement);
            connection.rollback();
            setError(message);
            if (error)
                *error = m_lastError;
            return false;
        }
    }
    if (!connection.commit()) {
        setError(connection.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

void Database::close()
{
    if (!m_connectionName.isEmpty()) {
        {
            // The copy has to be gone before the connection is removed, or Qt
            // warns that queries on it will stop working.
            QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
            if (connection.isOpen())
                connection.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
    }
    m_open = false;
    m_version = 0;
    m_path.clear();
}

bool Database::isEmpty() const
{
    return componentCount() == 0 && factCount() == 0;
}

int Database::componentCount() const
{
    if (!m_open)
        return 0;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM components")) || !query.next())
        return 0;
    return query.value(0).toInt();
}

int Database::factCount() const
{
    if (!m_open)
        return 0;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM facts")) || !query.next())
        return 0;
    return query.value(0).toInt();
}

QStringList Database::topics() const
{
    QStringList topics;
    if (!m_open)
        return topics;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (query.exec(QStringLiteral("SELECT DISTINCT topic FROM facts WHERE topic <> '' "
                                   "ORDER BY topic"))) {
        while (query.next())
            topics.append(query.value(0).toString());
    }
    return topics;
}

bool Database::upsertComponent(ComponentRecord *record, QString *error)
{
    if (record == nullptr) {
        setError(QStringLiteral("No record to store."));
        if (error)
            *error = m_lastError;
        return false;
    }
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }

    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    const QString now = isoString(QDateTime::currentDateTimeUtc());

    // A part number identifies a part: a record with the same part number and
    // manufacturer is the same record, so it is updated, not duplicated.
    if (record->uid.isEmpty() && !record->mpn.isEmpty()) {
        QSqlQuery find(connection);
        find.prepare(QStringLiteral("SELECT uid FROM components WHERE mpn = ? "
                                    "COLLATE NOCASE AND manufacturer = ? COLLATE NOCASE"));
        find.addBindValue(textValue(record->mpn));
        find.addBindValue(textValue(record->manufacturer));
        if (find.exec() && find.next())
            record->uid = find.value(0).toString();
    }

    QSqlQuery existing(connection);
    existing.prepare(QStringLiteral("SELECT uid, created FROM components WHERE uid = ?"));
    existing.addBindValue(record->uid);
    const bool known = record->uid.isEmpty() ? false : (existing.exec() && existing.next());
    if (record->uid.isEmpty())
        record->uid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const QString created = known ? existing.value(1).toString() : now;
    QSqlQuery query(connection);
    if (known) {
        query.prepare(QStringLiteral(
            "UPDATE components SET mpn = ?, manufacturer = ?, category = ?, description = ?, "
            "package = ?, datasheet_url = ?, datasheet_path = ?, notes = ?, source_title = ?, "
            "source_url = ?, source_path = ?, source_location = ?, status = ?, value_source = ?, "
            "retrieved = ?, created = ?, updated = ? WHERE uid = ?"));
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO components (mpn, manufacturer, category, description, package, "
            "datasheet_url, datasheet_path, notes, source_title, source_url, source_path, "
            "source_location, status, value_source, retrieved, created, updated, uid) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    }

    query.addBindValue(textValue(record->mpn));
    query.addBindValue(textValue(record->manufacturer));
    query.addBindValue(textValue(toString(record->category)));
    query.addBindValue(textValue(record->description));
    query.addBindValue(textValue(record->package));
    query.addBindValue(textValue(record->datasheetUrl));
    query.addBindValue(textValue(record->datasheetPath));
    query.addBindValue(textValue(record->notes));
    query.addBindValue(textValue(record->provenance.title));
    query.addBindValue(textValue(record->provenance.url));
    query.addBindValue(textValue(record->provenance.localPath));
    query.addBindValue(textValue(record->provenance.location));
    query.addBindValue(textValue(toString(record->provenance.status)));
    query.addBindValue(textValue(toString(record->provenance.valueSource)));
    query.addBindValue(textValue(isoString(record->provenance.retrieved)));
    query.addBindValue(textValue(created));
    query.addBindValue(textValue(now));
    query.addBindValue(textValue(record->uid));

    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }

    record->created = fromIso(created);
    record->updated = fromIso(now);
    emit changed();
    return true;
}

QVector<ComponentRecord> Database::findComponents(const QString &text, int limit) const
{
    QVector<ComponentRecord> records;
    if (!m_open)
        return records;

    const QString pattern = QStringLiteral("%%1%").arg(text.trimmed());
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM components WHERE mpn LIKE ? OR manufacturer LIKE ? OR "
        "description LIKE ? OR package LIKE ? OR notes LIKE ? "
        "ORDER BY updated DESC LIMIT ?"));
    for (int i = 0; i < 5; ++i)
        query.addBindValue(textValue(pattern));
    query.addBindValue(limit);
    if (!query.exec())
        return records;
    while (query.next())
        records.append(componentFromQuery(query));
    return records;
}

ComponentRecord Database::componentByUid(const QString &uid) const
{
    ComponentRecord record;
    if (!m_open || uid.isEmpty())
        return record;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT * FROM components WHERE uid = ?"));
    query.addBindValue(textValue(uid));
    if (query.exec() && query.next())
        record = componentFromQuery(query);
    return record;
}

bool Database::removeComponent(const QString &uid, QString *error)
{
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }
    QSqlDatabase connection = QSqlDatabase::database(m_connectionName);
    if (!connection.transaction()) {
        setError(connection.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    // The facts of a component are removed with it: a fact without a component
    // would look like general knowledge.
    QSqlQuery facts(connection);
    facts.prepare(QStringLiteral("DELETE FROM facts WHERE component_uid = ?"));
    facts.addBindValue(textValue(uid));
    if (!facts.exec()) {
        connection.rollback();
        setError(facts.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    QSqlQuery query(connection);
    query.prepare(QStringLiteral("DELETE FROM components WHERE uid = ?"));
    query.addBindValue(textValue(uid));
    if (!query.exec()) {
        connection.rollback();
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    connection.commit();
    emit changed();
    return true;
}

bool Database::addFact(Fact *fact, QString *error)
{
    if (fact == nullptr) {
        setError(QStringLiteral("No fact to store."));
        if (error)
            *error = m_lastError;
        return false;
    }
    if (fact->statement.trimmed().isEmpty()) {
        setError(QStringLiteral("A fact without a statement is not stored."));
        if (error)
            *error = m_lastError;
        return false;
    }
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO facts (component_uid, topic, statement, value, unit, conditions, "
        "source_title, source_publisher, source_url, source_path, source_location, status, "
        "value_source, retrieved, notes, created) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(textValue(fact->componentUid));
    query.addBindValue(textValue(fact->topic));
    query.addBindValue(textValue(fact->statement.trimmed()));
    query.addBindValue(textValue(fact->value));
    query.addBindValue(textValue(fact->unit));
    query.addBindValue(textValue(fact->conditions));
    query.addBindValue(textValue(fact->provenance.title));
    query.addBindValue(textValue(fact->provenance.publisher));
    query.addBindValue(textValue(fact->provenance.url));
    query.addBindValue(textValue(fact->provenance.localPath));
    query.addBindValue(textValue(fact->provenance.location));
    query.addBindValue(textValue(toString(fact->provenance.status)));
    query.addBindValue(textValue(toString(fact->provenance.valueSource)));
    query.addBindValue(textValue(isoString(fact->provenance.retrieved)));
    query.addBindValue(textValue(fact->provenance.notes));
    query.addBindValue(textValue(isoString(QDateTime::currentDateTimeUtc())));

    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    fact->id = query.lastInsertId().toLongLong();
    fact->created = QDateTime::currentDateTimeUtc();
    emit changed();
    return true;
}

bool Database::updateFact(const Fact &fact, QString *error)
{
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "UPDATE facts SET component_uid = ?, topic = ?, statement = ?, value = ?, unit = ?, "
        "conditions = ?, source_title = ?, source_publisher = ?, source_url = ?, source_path = ?, "
        "source_location = ?, status = ?, value_source = ?, retrieved = ?, notes = ? "
        "WHERE id = ?"));
    query.addBindValue(textValue(fact.componentUid));
    query.addBindValue(textValue(fact.topic));
    query.addBindValue(textValue(fact.statement.trimmed()));
    query.addBindValue(textValue(fact.value));
    query.addBindValue(textValue(fact.unit));
    query.addBindValue(textValue(fact.conditions));
    query.addBindValue(textValue(fact.provenance.title));
    query.addBindValue(textValue(fact.provenance.publisher));
    query.addBindValue(textValue(fact.provenance.url));
    query.addBindValue(textValue(fact.provenance.localPath));
    query.addBindValue(textValue(fact.provenance.location));
    query.addBindValue(textValue(toString(fact.provenance.status)));
    query.addBindValue(textValue(toString(fact.provenance.valueSource)));
    query.addBindValue(textValue(isoString(fact.provenance.retrieved)));
    query.addBindValue(textValue(fact.provenance.notes));
    query.addBindValue(fact.id);
    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    emit changed();
    return true;
}

bool Database::removeFact(qlonglong id, QString *error)
{
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("DELETE FROM facts WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    emit changed();
    return true;
}

QVector<Fact> Database::factsForComponent(const QString &componentUid, int limit) const
{
    QVector<Fact> facts;
    if (!m_open)
        return facts;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT * FROM facts WHERE component_uid = ? "
                                 "ORDER BY topic, id LIMIT ?"));
    query.addBindValue(textValue(componentUid));
    query.addBindValue(limit);
    if (!query.exec())
        return facts;
    while (query.next())
        facts.append(factFromQuery(query));
    return facts;
}

QVector<Fact> Database::recentFacts(int limit) const
{
    QVector<Fact> facts;
    if (!m_open)
        return facts;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT * FROM facts ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec())
        return facts;
    while (query.next())
        facts.append(factFromQuery(query));
    return facts;
}

QString Database::toMatchQuery(const QString &userText)
{
    // The user types words, not a query language: every word becomes a quoted
    // token, so quotes, stars and operators in the input can do nothing. The
    // last word is a prefix, so a search completes as it is typed.
    QStringList tokens;
    QString current;
    for (const QChar c : userText) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-')
            || c == QLatin1Char('.')) {
            current.append(c);
        } else if (!current.isEmpty()) {
            tokens.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty())
        tokens.append(current);

    QStringList quoted;
    for (int i = 0; i < tokens.size(); ++i) {
        QString token = tokens.at(i);
        if (token == QLatin1String("-") || token == QLatin1String("."))
            continue;
        token.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        const bool last = i == tokens.size() - 1;
        quoted.append(QStringLiteral("\"%1\"%2").arg(token, last ? QStringLiteral("*") : QString()));
    }
    return quoted.join(QStringLiteral(" AND "));
}

QVector<Fact> Database::searchFacts(const QString &query, int limit) const
{
    m_lastSearchMode = QStringLiteral("recent");
    if (!m_open)
        return QVector<Fact>();

    const QString match = toMatchQuery(query);
    if (match.isEmpty())
        return recentFacts(limit);

    QSqlQuery search(QSqlDatabase::database(m_connectionName));
    search.prepare(QStringLiteral(
        "SELECT facts.*, bm25(facts_fts, 5.0, 2.0, 1.0) AS rank "
        "FROM facts_fts JOIN facts ON facts.id = facts_fts.rowid "
        "WHERE facts_fts MATCH ? ORDER BY rank LIMIT ?"));
    search.addBindValue(textValue(match));
    search.addBindValue(limit);
    if (search.exec()) {
        QVector<Fact> facts;
        while (search.next())
            facts.append(factFromQuery(search));
        m_lastSearchMode = QStringLiteral("fts5");
        return facts;
    }

    // The index could not answer this one. A plain scan does, and the reason is
    // logged: a search that silently returns nothing would be worse.
    logWarning(QStringLiteral("database"),
               QStringLiteral("full text search failed (%1), falling back to a text scan: %2")
                   .arg(search.lastError().text(), query));
    const QStringList tokens = match.split(QStringLiteral(" AND "));
    QSqlQuery fallback(QSqlDatabase::database(m_connectionName));
    QString sql = QStringLiteral("SELECT * FROM facts WHERE statement LIKE ?");
    for (int i = 1; i < tokens.size(); ++i)
        sql += QStringLiteral(" AND statement LIKE ?");
    sql += QStringLiteral(" ORDER BY id DESC LIMIT ?");
    fallback.prepare(sql);
    for (const QString &token : tokens) {
        QString word = token;
        word.remove(QLatin1Char('"'));
        word.remove(QLatin1Char('*'));
        fallback.addBindValue(QStringLiteral("%%1%").arg(word));
    }
    fallback.addBindValue(limit);
    QVector<Fact> facts;
    if (fallback.exec()) {
        while (fallback.next())
            facts.append(factFromQuery(fallback));
    }
    m_lastSearchMode = QStringLiteral("like");
    return facts;
}

bool Database::checkIntegrity(QString *error) const
{
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("PRAGMA integrity_check"))) {
        setError(query.lastError().text());
        if (error)
            *error = m_lastError;
        return false;
    }
    if (query.next()) {
        const QString result = query.value(0).toString();
        if (result.compare(QLatin1String("ok"), Qt::CaseInsensitive) != 0) {
            setError(result);
            if (error)
                *error = m_lastError;
            return false;
        }
    }
    // The full text index has its own consistency check.
    QSqlQuery fts(QSqlDatabase::database(m_connectionName));
    fts.prepare(QStringLiteral("INSERT INTO facts_fts(facts_fts) VALUES('integrity-check')"));
    if (!fts.exec()) {
        setError(QStringLiteral("the search index is damaged: %1").arg(fts.lastError().text()));
        if (error)
            *error = m_lastError;
        return false;
    }
    return true;
}

bool Database::vacuum(QString *error)
{
    if (!m_open) {
        setError(QStringLiteral("The knowledge database is not open."));
        if (error)
            *error = m_lastError;
        return false;
    }
    return run(QStringLiteral("VACUUM"), error);
}

} // namespace eda
