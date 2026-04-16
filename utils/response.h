#pragma once
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

namespace Response {

inline QByteArray success(const QJsonObject &data = {}, const QString &message = "OK") {
    QJsonObject resp;
    resp["success"] = true;
    resp["message"] = message;
    if (!data.isEmpty()) resp["data"] = data;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

inline QByteArray successArray(const QJsonArray &data, const QString &message = "OK") {
    QJsonObject resp;
    resp["success"] = true;
    resp["message"] = message;
    resp["data"] = data;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

inline QByteArray error(int code, const QString &message) {
    QJsonObject resp;
    resp["success"] = false;
    resp["error"] = QJsonObject{{"code", code}, {"message", message}};
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

// HTTP Status codes
constexpr int HTTP_OK         = 200;
constexpr int HTTP_CREATED    = 201;
constexpr int HTTP_NO_CONTENT = 204;
constexpr int HTTP_BAD_REQ    = 400;
constexpr int HTTP_UNAUTH     = 401;
constexpr int HTTP_FORBIDDEN  = 403;
constexpr int HTTP_NOT_FOUND  = 404;
constexpr int HTTP_CONFLICT   = 409;
constexpr int HTTP_TOO_LARGE  = 413;
constexpr int HTTP_SERVER_ERR = 500;

} // namespace Response
