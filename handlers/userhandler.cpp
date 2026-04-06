#include "userhandler.h"
#include "utils/response.h"

#include <QJsonParseError>

UserHandler::UserHandler(DatabaseManager *db, AuthMiddleware *auth, QObject *parent)
  : QObject(parent), m_db(db), m_auth(auth)
{
}

QPair<int, QByteArray> UserHandler::handleGetProfile(const QString &authHeader)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user) return {Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    return {Response::HTTP_OK, Response::success(user->toJson())};
}

QPair<int, QByteArray> UserHandler::handleUpdateProfile(const QString &authHeader, const QByteArray &body)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
        if (!user) return {Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    // Проверка на ошибки в jsonе
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "Invalid JSON")};

    QJsonObject obj = doc.object();
    //TO DO
}

QPair<int, QByteArray> UserHandler::handleGetQuota(const QString &authHeader)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user) return {Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    auto used = m_db->calcUsedBytes(token->userId);
    user->usedBytes = used;

    QJsonObject quota;
    quota["totalBytes"] = user->quotaBytes;
    quota["usedBytes"] = user->usedBytes;
    //quota["freeBytes"] = user->quotaBytes - user->usedBytes;

    return {Response::HTTP_OK, Response::success(quota)};
}
