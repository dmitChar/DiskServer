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
        (id             INTEGER    PRIMARY KEY  AUTOINCREMENT,
        owner_id        INTEGER    NOT NULL     REFERENCES users(id) ON DELETE CASCADE,
        name            TEXT       NOT NULL,
        path            TEXT       NOT NULL,
        storage_path    TEXT       NOT NULL,
        type            TEXT       NOT NULL     DEFAULT 'file',
        size_bytes      TEXT       NOT NULL     DEFAULT 0,
        mime_type       TEXT                    DEFAULT 'application/octet-stream',
        checksum        TEXT,
        is_shared       INTEGER    NOT NULL     DEFAULT 0,
        share_token     TEXT,
        created_at      TEXT       NOT NULL,
        updated_at      TEXT       NOT NULL,
        last_accessed   TEXT       NOT NULL,
        UNIQUE(owner_id, path)
        )
    )"))
    {
        qCritical() << "[DB] files error:" << query.lastError().text();
        return false;
    }

    //Таблица сессий
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS sessions (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id    INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
            token      TEXT    NOT NULL UNIQUE,
            expires_at INTEGER NOT NULL,
            created_at TEXT    NOT NULL
        )
    )")) {
        qCritical() << "[DB] sessions table:" << query.lastError().text();
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

void DatabaseManager::createSession(qint64 userId, const QString &jwtToken, qint64 expiresAt)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO sessions (user_id, token, expires_at, created_at)
        VALUES (:uid, :t, :exp, :ca)
    )");
    q.bindValue(":uid", userId);
    q.bindValue(":t",   jwtToken);
    q.bindValue(":exp", expiresAt);
    q.bindValue(":ca",  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!q.exec())
        qDebug() << "[DB] createSession error:" << q.lastError().text();
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
    q.prepare(R"(SELECT SUM(size_bytes) FROM files WHERE owner_id = :id)");
    q.bindValue(":id", id);

    if (!q.exec() || !q.next())
    {
        qDebug() << "[DB] calcUsedBytes error:" << q.lastError().text();
        return 0;
    }
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

// ─── File Data ────────────────────────────────────────────────────────────

/**
 * @brief fileFromQuery Вспомогательная функция для составляения объекта структуры FileData из запроса к бд
 * @param q результат запроса к бд
 * @return заполненный объект FileData
 */
static FileData fileFromQuery(const QSqlQuery &q) {
    FileData f;
    f.id           = q.value("id").toLongLong();
    f.ownerId      = q.value("owner_id").toLongLong();
    f.name         = q.value("name").toString();
    f.path         = q.value("path").toString();
    f.serverPath   = q.value("storage_path").toString();
    f.type         = q.value("type").toString() == "directory"
                       ? FileType::Directory : FileType::File;
    f.sizeBytes    = q.value("size_bytes").toLongLong();
    f.mimeType     = q.value("mime_type").toString();
    f.checkSum     = q.value("checksum").toString();
    f.isShared     = q.value("is_shared").toBool();
    f.shareToken   = q.value("share_token").toString();
    f.createdAt    = QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate);
    f.updatedAt    = QDateTime::fromString(q.value("updated_at").toString(), Qt::ISODate);
    f.lastAccessed = QDateTime::fromString(q.value("last_accessed").toString(), Qt::ISODate);
    return f;
}


// Добавление нового файла в бд
// Возвращает созданный файл в виде "FileData"
optional<FileData> DatabaseManager::createFile(const FileData &data)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
              INSERT INTO files
                  (owner_id, name, path, storage_path, type, size_bytes,
                   mime_type, checksum, is_shared, share_token,
                   created_at, updated_at, last_accessed)
              VALUES
                  (:oid, :n, :p, :sp, :t, :sb,
                   :mt, :cs, :isd, :st,
                   :ca, :ua, :la)
              )");
    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    q.bindValue(":oid", data.ownerId);
    q.bindValue(":n", data.name);
    q.bindValue(":p", data.path);
    q.bindValue(":sp", data.serverPath);
    q.bindValue(":t", data.isDirectory() ? "directory" : "file");
    q.bindValue(":sb", data.sizeBytes);
    q.bindValue(":mt", data.mimeType);
    q.bindValue(":cs", data.checkSum);
    q.bindValue(":isd", data.isShared);
    q.bindValue(":st", data.shareToken.isEmpty() ? QVariant() : data.shareToken);
    q.bindValue(":ca", now);
    q.bindValue(":ua", now);
    q.bindValue(":la", now);

    if (!q.exec())
    {
        qWarning() << "[DB] createFile" << q.lastError().text();
        return nullopt;
    }
    updateUserUsedBytes(data.ownerId, calcUsedBytes(data.ownerId));
    return getFileById(q.lastInsertId().toLongLong());
}

// Получение файла по его id
optional<FileData> DatabaseManager::getFileById(qint64 id)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM files WHERE id = :id");
    q.bindValue(":id", id);

    if (q.exec() && q.next())
        return fileFromQuery(q);

    return nullopt;
}

bool DatabaseManager::updateFile(const FileData &meta)
{
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE files SET
            name=:n, path=:p, storage_path=:sp, size_bytes=:sb,
            mime_type=:mt, checksum=:cs, is_shared=:isd,
            share_token=:st, updated_at=:ua
        WHERE id=:id
    )");
    q.bindValue(":n",   meta.name);
    q.bindValue(":p",   meta.path);
    q.bindValue(":sp",  meta.serverPath);
    q.bindValue(":sb",  meta.sizeBytes);
    q.bindValue(":mt",  meta.mimeType);
    q.bindValue(":cs",  meta.checkSum);
    q.bindValue(":isd", meta.isShared ? 1 : 0);
    q.bindValue(":st",  meta.shareToken.isEmpty() ? QVariant() : meta.shareToken);
    q.bindValue(":ua",  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id",  meta.id);

    return q.exec();
}

bool DatabaseManager::deleteFileById(const qint64 &fileId)
{

}

// Получение файла по его пути
optional<FileData> DatabaseManager::getFileByPath(qint64 ownerId, const QString &path)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM files WHERE owner_id = :oid AND path = :p");
    q.bindValue(":oid", ownerId);
    q.bindValue(":p", path);

    if (q.exec() && q.next())
        return fileFromQuery(q);

    return nullopt;
}

// Получение только ПРЯМЫХ потомков для dirPath
QVector<FileData> DatabaseManager::listDirectory(qint64 ownerId, const QString &dirPath)
{
    QSqlQuery q(m_db);
    QString prefix = dirPath.endsWith('/') ? dirPath : dirPath + "/";

    q.prepare(R"(
              SELECT * FROM files
              WHERE owner_id = :oid
                AND path LIKE :prefix ESCAPE '\'
                AND path NOT LIKE :deeper ESCAPE '\'
              ORDER BY type DESC, name ASC
              )");

    QString likePrefix = prefix.replace("%", "\%").replace("_", "\_") + "_%";

    QString notLike = likePrefix + "/_%";
    q.bindValue(":oid", ownerId);
    q.bindValue(":prefix", likePrefix);
    q.bindValue(":deeper", notLike);

    QVector<FileData> result;
    if (q.exec())
    {
        while (q.next())
            result.append(fileFromQuery(q));
    }
    return result;
}

optional<qint64> DatabaseManager::getFileIdByPath(const QString &path)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM files WHERE path = :path");
    q.bindValue(":path", path);

    if (!q.exec() || !q.next())
    {
        return nullopt;
    }
    return q.value("id").toULongLong();
}

optional<qint64> DatabaseManager::getOwnerIdById(const qint64 fileId)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT owner_id FROM files WHERE id = :id");
    q.bindValue(":id", fileId);

    if (!q.exec() || !q.next())
    {
        return nullopt;
    }
    return q.value("owner_id").toLongLong();
}

bool DatabaseManager::updateUserUsedBytes(qint64 userId, qint64 usedBytes)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE users SET used_bytes = :ub, updated_at = :ua WHERE id = :id");
    q.bindValue(":ub", usedBytes);
    q.bindValue(":ua", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    q.bindValue(":id", userId);

    return q.exec();
}
