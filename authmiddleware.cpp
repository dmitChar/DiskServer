#include "authmiddleware.h"

AuthMiddleware::AuthMiddleware(DatabaseManager *db, const QString &jwtSecret):
    m_db(db), m_jwtSecret(jwtSecret)
{
}

std::optional<TokenPayload> AuthMiddleware::authencticate(const QString &authHeader)
{
    if (!authHeader.startsWith("Bearer", Qt::CaseInsensitive))
    {
        return std::nullopt;
    }
    QString token = authHeader.mid(7).trimmed();
    if (token.isEmpty())
        return std::nullopt;

    //Verify jwt and decode
    auto payload = JwtUtils::verifyToken(token, m_jwtSecret);
    if (!payload) return std::nullopt;

    //Check expiry
    if (JwtUtils::isExpired(*payload)) return std::nullopt;

    //check session in db
    if (!m_db->isSessionValid(token))
        return std::nullopt;
}

QString AuthMiddleware::jwtSecret() const
{
    return m_jwtSecret;
}
