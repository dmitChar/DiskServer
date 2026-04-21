#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlQuery>
#include <optional>

#include "utils/user.h"
#include "utils/filedata.h"

using  std::optional;
using std::nullopt;

class DatabaseManager : public QObject
{
public:
    DatabaseManager(QObject *parent);
    bool open(const QString &dbName);
    bool isOpen() const;
    void close();

    //---------Users---------
    optional<User> getUserByUsername(const QString &username);
    optional<User> createUser(const QString &username, const QString &email, const QString &hash, const QString &salt, qint64 defaultQuota);
    optional<User> getUserById(const qint64 &id);
    qint64 calcUsedBytes(const qint64 &id);


    //---------Sessions---------
    void createSession(qint64 userId, const QString &jwtToken, qint64 expiresAt);
    bool deleteSession(const QString &jwtToken);
    bool isSessionValid(const QString &jwtToken);

    //--------- Files Data ---------
    optional<FileData> createFile(const FileData &data);
    optional<FileData> getFileById(qint64 id);
    optional<FileData> getFileByPath(qint64 ownerId, const QString &path);
    QVector<FileData> listDirectory(qint64 ownerId, const QString &dirPath);
    optional<qint64> getFileIdByPath(const QString &path);
    optional<qint64> getOwnerIdById(const qint64 fileId);
    bool updateUserUsedBytes(qint64 userId, qint64 usedBytes);
    bool updateFile(const FileData &meta);
    bool deleteFileById(const qint64 &fileId);

private:
    bool createTables();
    QSqlDatabase m_db;
    QString m_connectionName;
};

#endif // DATABASEMANAGER_H
