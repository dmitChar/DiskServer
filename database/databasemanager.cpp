#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

DatabaseManager::DatabaseManager(QObject *parent):QObject(parent)
{
    m_connectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool DatabaseManager::open(const QString &dbPath)
{
    m_db = QSqlDatabase.addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.isOpen())
    {
        qCritical() << "DB" << dbPath << "is not open!: " << m_db.lastError().text();
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
        (id INTEGER    PRIMARY KEY    AUTOINCREMENT,
        username    TEXT    NOT NULL    UNIQUE,
        email   TEXT    NOT NULL    UNIQUE,
        password_hash   TEXT NOT NULL,
        salt TEXT   NOT NULL,
        quota_bytes NOT NULL    DEFAULT 10737418240,
        used_bytes  NOT NULL    DEFAULT 0,
        is_active   NOT NULL    DEFAULT 1,
        created_at  NOT NULL,
        updated_at  NOT NULL
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

    qDebug() << "All tables was loaded";
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
    QSqlDatabase::removeDatabase(m_connectionName);
}
