#ifndef AUTHHANDLER_H
#define AUTHHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QPair>

#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"

class AuthHandler : public QObject
{
    Q_OBJECT
public:
    explicit AuthHandler(DatabaseManager *db,
                         AuthMiddleware *auth,
                         qint64 defaultQuotaBytes,
                         int tokenExpiryHours,
                         QObject *parent = nullptr);

    //Returns {statusCode, responseBody}
    QPair<int, QByteArray> handleRegister(const QByteArray &body);
    QPair<int, QByteArray> handleLogin(const QByteArray &body);
    QPair<int, QByteArray> handleLogout(const QString &authHeader);
    QPair<int, QByteArray> handleMe(const QString &authHeader);
    QPair<int, QByteArray> handleChangePassword(const QString &authHeader, const QByteArray &body);

private:
    DatabaseManager *m_db;
    AuthMiddleware *m_auth;
    qint64 m_defaultQuota;
    int m_tokenExpiryHours;
};

#endif // AUTHHANDLER_H
