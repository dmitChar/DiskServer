#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QUuid>
#include <QDebug>
#include <QDateTime>
#include <QDir>

DatabaseManager::DatabaseManager(QObject *parent)
    :QObject(parent)
{
    m_connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// ─── Tables ──────────────────────────────────────────────────────────────────

bool DatabaseManager::open(const QString &dbName)
{
    QString path = QDir::currentPath();
    QDir dir(path);
    if (!dir.exists())
        dir.mkpath(".");

    QString dbFile = path + "/" + dbName;
    qDebug() << "[DB] path" << dbFile;

    if (!QSqlDatabase::contains("CloudDiskDB"))
    {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }
    else
    {
        m_db = QSqlDatabase::database("CloudDiskDB");
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbName);

    if (!m_db.open())
    {
        qCritical() << "[DB]" << dbFile << "is not open!: " << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);

    //Таблица пользователей
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS users
        (id             INTEGER    PRIMARY KEY    AUTOINCREMENT,
        username        TEXT    NOT NULL    UNIQUE,
        email           TEXT    NOT NULL    UNIQUE,
        password_hash   TEXT    NOT NULL,
        salt            TEXT    NOT NULL,
        quota_bytes     INTEGER NOT NULL    DEFAULT 10737418240,
        used_bytes      INTEGER NOT NULL    DEFAULT 0,
        is_active       BOOL    NOT NULL    DEFAULT 1,
        created_at      TEXT    NOT NULL,
        updated_at      TEXT    NOT NULL
        )
    )"))
    {
        qCritical() << "[DB] users error:" << query.lastError().text();
        return false;
    }

    //Таблица файлов
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS files
        (id INTEGER PRIMARY KEY AUTOINCREMENT,
        owner_id    INTEGER    NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        name    TEXT    NOT NULL,
        path    TEXT    NOT NULL,
        storage_path    TEXT    NOT NULL,
        type    TEXT    NOT NULL DEFAULT 'file',
        size_butes      TEXT    NOT NULL     DEFAULT 0,
        mime_type   TEXT    NOT NULL    DEFAULT 'application/octet-stream',
        checksum    TEXT,
        is_shared   INTEGER    NOT NULL     DEFAULT 0,
        share_token     TEXT    NOT NULL,
        created_at    TEXT    NOT NULL,
        updated_at    TEXT    NOT NULL,
        last_accessed TEXT    NOT NULL,
        UNIQUE(owner_id, path)
        )
    )"))
    {
        qCritical() << "[DB] files error:" << query.lastError().text();
        return false;
    }

    //Таблица сессий
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS sessions
        (id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        token TEXT NOT NULL UNIQUE,
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
        )
    )"))
    {
        qCritical() << "[DB] sessions error:" <<  query.lastError().text();
        return false;
    }

    qDebug() << "[DB] All tables was loaded";
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
    QSqlDatabase::removeDatabase(m_connectionName);
}

// ─── Sessions ─────────────────────────────────────────────────────────────────

bool DatabaseManager::createSession(qint64 &userId, const QString &jwtToken, qint64 expiresAt)
{
    QSqlQuery q(m_db);
    q.prepare(R"(INSERT INTO sessions (user_id, token, expires_at, created_at)
                VALUES (:uid, :t, :exp, :ca)
                )");

    q.bindValue(":uid", userId);
    q.bindValue(":t", jwtToken);
    q.bindValue(":exp", expiresAt);
    q.bindValue(":ca", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return q.exec();
}

bool DatabaseManager::deleteSession(const QString &jwtToken)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM sessions WHERE token = :t");
    q.bindValue(":t", jwtToken);

    return q.exec();
}

bool DatabaseManager::isSessionValid(const QString &jwtToken)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT expires_at FROM sessions WHERE token = :t");
    q.bindValue(":t", jwtToken);

    if (!q.exec() || !q.next())
        return false;
    qint64 exp = q.value(0).toLongLong();

    return QDateTime::currentSecsSinceEpoch() < exp;
}

// ─── Users ───────────────────────────────────────────────────────────────────

static User userFromQuery(const QSqlQuery &q)
{
    User u;
    u.id = q.value("id").toLongLong();
    u.username = q.value("username").toString();
    u.email = q.value("email").toString();
    u.passwordHash = q.value("password_hash").toString();
    u.salt = q.value("salt").toString();
    u.quotaBytes = q.value("quota_bytes").toLongLong();
    u.usedBytes = q.value("used_bytes").toLongLong();
    u.isActive = q.value("is_active").toBool();
    u.createdAt = QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate);
    u.updatedAt = QDateTime::fromString(q.value("updated_at").toString(), Qt::ISODate);

    return u;
}

optional<User> DatabaseManager::getUserByUsername(const QString &username)
{
    QSqlQuery query(m_db);
    query.prepare(R"(SELECT *FROM users WHERE username = :u)");
    query.bindValue(":u", username);

    if (query.exec() && query.next())
        return userFromQuery(query);
    return nullopt;
}

optional<User> DatabaseManager::getUserById(const qint64 &id)
{
    QSqlQuery q(m_db);
    q.prepare(R"(SELECT * FROM users WHERE id = :id)");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return userFromQuery(q);
    return nullopt;
}

qint64 DatabaseManager::calcUsedBytes(const qint64 &id)
{
    QSqlQuery q(m_db);
    q.prepare(R"(SELECT used_bytes FROM users WHERE id = :id)");
    q.bindValue(":id", id);

    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toLongLong();
}

optional<User> DatabaseManager::createUser(const QString &username, const QString &email,
                                                const QString &hash, const QString &salt,
                                                qint64 defaultQuota)
{
    QSqlQuery query(m_db);
    query.prepare(R"(INSERT INTO users (username, email, password_hash, salt, quota_bytes, created_at, updated_at)
                  VALUES (:u, :e, :ph, :s, :qb, :ca, :ua)
                  )");
    QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    query.bindValue(":u", username);
    query.bindValue(":e", email);
    query.bindValue(":ph", hash);
    query.bindValue(":s", salt);
    query.bindValue(":qb", defaultQuota);
    query.bindValue(":ca", now);
    query.bindValue(":ua", now);

    if (!query.exec())
    {
        qDebug() << "[DB] createUser:" << query.lastError().text();
        return nullopt;
    }
    return getUserById(query.lastInsertId().toLongLong());
}

// ─── File MetaData ────────────────────────────────────────────────────────────
