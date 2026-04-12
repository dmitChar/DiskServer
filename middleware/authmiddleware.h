#ifndef AUTHMIDDLEWARE_H
#define AUTHMIDDLEWARE_H

#include <QObject>
#include "utils/jwtutils.h"
#include "database/databasemanager.h"

#include <optional>

using std::optional;
using std::nullopt;

class AuthMiddleware
{
public:
    explicit AuthMiddleware(DatabaseManager *db, const QString &jwtSecret);

    optional<TokenPayload> authencticate(const QString &authHeader);
    QString jwtSecret() const
    {
        return m_jwtSecret;
    }

private:
    DatabaseManager *m_db;
    QString m_jwtSecret;
};

#endif // AUTHMIDDLEWARE_H
