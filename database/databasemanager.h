#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlQuery>
#include <optional>

#include "utils/user.h"

using std::optional;
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
    bool createSession(qint64 &userId, const QString &jwtToken, qint64 expiresAt);
    bool deleteSession(const QString &jwtToken);
    bool isSessionValid(const QString &jwtToken);

private:
    bool createTables();
    QSqlDatabase m_db;
    QString m_connectionName;
};

#endif // DATABASEMANAGER_H
