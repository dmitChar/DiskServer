#ifndef USERHANDLER_H
#define USERHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QPair>

#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"

class UserHandler : public QObject
{
    Q_OBJECT
public:
    explicit UserHandler(DatabaseManager *db, AuthMiddleware *auth,QObject *parent = nullptr);

    // GET /api/user/profile
    QPair<int, QByteArray> handleGetProfile(const QString &authHeader);

    //PUT /api/user/profile body:{"email":"..."}
    QPair<int, QByteArray> handleUpdateProfile(const QString &authHeader, const QByteArray &body);

    // GET /api/user/quota
    QPair<int, QByteArray> handleGetQuota(const QString &authHeader);

private:
    DatabaseManager *m_db;
    AuthMiddleware *m_auth;

};

#endif // USERHANDLER_H
