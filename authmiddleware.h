#ifndef AUTHMIDDLEWARE_H
#define AUTHMIDDLEWARE_H

#include <QObject>
#include "utils/jwtutils.h"
#include "database/databasemanager.h"



class AuthMiddleware
{
public:
    explicit AuthMiddleware(DatabaseManager *db, const QString &jwtSecret);

    std::optional<TokenPayload> authencticate(const QString &authHeader);
    QString jwtSecret() const;

private:
    DatabaseManager *m_db;
    QString m_jwtSecret;
};

#endif // AUTHMIDDLEWARE_H
