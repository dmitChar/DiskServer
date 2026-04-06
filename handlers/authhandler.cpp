#include "authhandler.h"
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>

#include "utils/jwtutils.h"
#include "utils/response.h"
#include "utils/hashutils.h"

AuthHandler::AuthHandler(DatabaseManager *db, AuthMiddleware *auth, qint64 defaultQuotaBytes,
                          int tokenExpiryHours = 5, QObject *parent) :
    QObject(parent), m_db(db), m_auth(auth),
    m_defaultQuota(defaultQuotaBytes), m_tokenExpiryHours(tokenExpiryHours)
{

}

//POST /api/auth/register
QPair<int, QByteArray> AuthHandler::handleRegister(const QByteArray &body)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "INVALID JSON")};

    QJsonObject obj = doc.object();
    QString username = obj["username"].toString().trimmed();
    QString email = obj["email"].toString().trimmed();
    QString password = obj["password"].toString();

    //Validation
    if (username.isEmpty() || email.isEmpty() || password.isEmpty())
        return {Response::HTTP_BAD_REQ, Response::error(400, "username, email or password are invalid")};

    if (username.length() < 3 || username.length() > 32)
        return {Response::HTTP_BAD_REQ, Response::error(400, "username length invalid")};

    if (password.length() < 5)
        return {Response::HTTP_BAD_REQ, Response::error(400, "password is too short, must be at least 8 chars")};

    //Unique check
    if (m_db->getUserByUsername(username))
        return {Response::HTTP_CONFLICT, Response::error(409, "Username already exists")};
//    if (m_db->getUserByEmail(email))
//        return {Response::HTTP_CONFLICT, Response::error(409, "Email already exists")};

    QString salt = HashUtils::generateSalt();
    QString hash = HashUtils::hashPassword(password, salt);

    // Добавление в бд
    auto user = m_db->createUser(username, email, hash, salt, m_defaultQuota);
    if (!user)
        return {Response::HTTP_CONFLICT, Response::error(500, "Failed to create user")};

    qDebug() << "[Auth] << User was registered:" << username;
    return {Response::HTTP_CREATED, Response::success(user->toJson(), "User was registered")};
}

// POST /api/auth/login
QPair<int, QByteArray> AuthHandler::handleLogin(const QByteArray &body)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "INVALID JSON")};

    QJsonObject obj = doc.object();
    QString login = obj["username"].toString().trimmed();
    QString password = obj["password"].toString();

    if (login.isEmpty() || password.isEmpty())
        return {Response::HTTP_BAD_REQ, Response::error(400, "username or password is empty")};

    //Поиск пользователя в бд по логину
    auto user = m_db->getUserByUsername(login);
    if (!user)
        return {Response::HTTP_UNAUTH, Response::error(401, "Invalid credentials")};

    //Проверка на блокировку аккаунта
    if (!user->isActive)
        return {Response::HTTP_FORBIDDEN, Response::error(403, "Account is disabled")};

    //Проверка пароля
    if (!HashUtils::verifyPassword(password, user->passwordHash, user->salt))
        return {Response::HTTP_UNAUTH, Response::error(401, "Invalid credentials")};

    //Создание JWT
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 expires = now + m_tokenExpiryHours * 3600;
    TokenPayload payload{user->id, user->username, now, expires};
    QString token = JwtUtils::createToken(payload, m_auth->jwtSecret());

    QJsonObject resp;
    resp["token"] = token;
    resp["expiresAt"] = QDateTime::fromSecsSinceEpoch(expires).toString(Qt::ISODate);
    resp["user"] = user->toJson();

    //Запись сессии в бд
    m_db->createSession(user->id, token, expires);

    qDebug() << "[Auth] Login" << user->username;
    return {Response::HTTP_OK, Response::success(resp, "Login succesfully")};
}


// POST /api/auth/logout
QPair<int, QByteArray> AuthHandler::handleLogout(const QString &authHeader)
{
    auto payload =m_auth->authencticate(authHeader);
    if (!payload)
        return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    QString token = authHeader.mid(7).trimmed();
    m_db->deleteSession(token);

    return {Response::HTTP_OK, Response::success({}, "Log out successfully")};
}

// GET /api/auth/me
QPair<int, QByteArray> AuthHandler::handleMe(const QString &authHeader)
{
    auto payload = m_auth->authencticate(authHeader);
    if (!payload)
        return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(payload->userId);
    if (!user)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    return {Response::HTTP_OK, Response::success(user->toJson())};
}

// PUT /api/auth/password
QPair<int, QByteArray> AuthHandler::handleChangePassword(const QString &authHeader, const QByteArray &body)
{
    auto payload = m_auth->authencticate(authHeader);
    if (!payload)

    QJsonParseError err;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "Invalid Json")};

    QJsonObject obj = doc.object();
    QString currentPassword = obj["currentPassword"].toString().trimmed();
    QString newPassword = obj["newPassword"].toString();

    //Validation
    if (currentPassword.isEmpty() || newPassword.isEmpty() )
        return {Response::HTTP_BAD_REQ, Response::error(400, "currentPassword or newPassword is empty")};
    if (newPassword.length() < 8)
        return {Response::HTTP_BAD_REQ, Response::error(400, "New password must be at least 8 characters")};

    auto user = m_db->getUserById(payload->userId);
    if (!user)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    if (!HashUtils::verifyPassword(currentPassword, user->passwordHash, user->salt))
        return {Response::HTTP_UNAUTH, Response::error(401, "Current password is incorrect")};

    QString newSalt = HashUtils::generateSalt();
    QString newHash = HashUtils::hashPassword(newPassword, newSalt);
    //m_db->deleteAllUserSessions(user->id);

    qDebug() << "Password for user" << user->username << "was changed";
    return {Response::HTTP_OK, Response::success({}, "Password chanhed")};
}
