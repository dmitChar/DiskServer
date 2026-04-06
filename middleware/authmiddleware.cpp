#include "authmiddleware.h"

AuthMiddleware::AuthMiddleware(DatabaseManager *db, const QString &jwtSecret)
    :m_db(db), m_jwtSecret(jwtSecret)
{

}

optional<TokenPayload> AuthMiddleware::authencticate(const QString &authHeader)
{
    if (!authHeader.startsWith("Bearer"),  Qt::CaseInsensitive)
        return nullopt;

    QString token = authHeader.mid(7).trimmed();
    if (token.isEmpty())
        return nullopt;

    // Проверка jwt токена
    auto payload = JwtUtils::verifyToken(token, m_jwtSecret);
    if (!payload)
        return nullopt;

    //Проверка срока годности
    if (JwtUtils::isExpired(*payload))
        return nullopt;

    if (!m_db->isSessionValid(token))
        return nullopt;

    return payload;
}
