#pragma once
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <optional>

using std::optional;
using std::nullopt;

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

//--- Range request parsing
struct RangeRequest
{
    qint64 first = 0;
    qint64 last = -1; // до конца файла по умолчанию

    qint64 length() const
    {
        return last - first + 1;
    }

    QString contentRangeHeader(qint64 fileSize) const
    {
        return QString("bytes %1-%2/%3").arg(first).arg(last).arg(fileSize);
    }

    // Для вычисления реальных размеров (например при first < 0)
    bool resolve(qint64 fileSize)
    {
        if (fileSize <= 0)
            return false;

        if (first < 0)
        {
            first = fileSize + first;
            last = fileSize -1;
        }
        else if (last < 0 || last >= fileSize)
            last = fileSize -1; // Открытый диапазон или за пределами

        return first >=0 && first <=last && last < fileSize;
    }
};


// Разбирает заголовок Range: bytes=<spec>
// Поддерживает:
// bytes = 0-1023 -> {0, 1023}
// bytes = 1024-  -> {1024, -1} (до конца)
// bytes = -512   -> {-512, -1} (суффикс: последние 512)
// Возрващает nullopt если заголовок пустой или неверного формата
inline optional<RangeRequest> parseRange(const QString &rangeHeader)
{
    if (rangeHeader.isEmpty())
        return nullopt;

    if (!rangeHeader.startsWith("bytes=", Qt::CaseInsensitive))
        return nullopt;

    QString spec = rangeHeader.mid(6).trimmed(); // убрать "bytes="

    if (spec.contains(','))
        return nullopt;

    RangeRequest req;
    if (spec.startsWith('-'))
    {
        bool ok = false;
        qint64 n = spec.toLongLong(&ok);
        if (!ok || n >=0)
            return nullopt;

        req.first = n;
        req.last = -1;
    }
    else
    {
        int dash = spec.indexOf('-');
        if (dash < 0)
            return nullopt;

        QString firstStr = spec.left(dash).trimmed();
        QString lastStr = spec.mid(dash + 1).trimmed();

        bool ok = false;
        req.first = firstStr.toLongLong(&ok);
        if (!ok || req.first < 0)
            return nullopt;

        if (lastStr.isEmpty())
            req.last = -1; // Открытый диапазон
        else
        {
            req.last = lastStr.toLongLong(&ok);
            if (!ok || req.last < req.first)
                return nullopt;
        }
    }
    return req;
}



// HTTP Status codes
constexpr int HTTP_OK         = 200;
constexpr int HTTP_CREATED    = 201;
constexpr int HTTP_NO_CONTENT = 204;
constexpr int HTTP_PARRIAL_CONTENT = 206;
constexpr int HTTP_BAD_REQ    = 400;
constexpr int HTTP_UNAUTH     = 401;
constexpr int HTTP_FORBIDDEN  = 403;
constexpr int HTTP_NOT_FOUND  = 404;
constexpr int HTTP_CONFLICT   = 409;
constexpr int HTTP_TOO_LARGE  = 413;
constexpr int HTTP_RANGE_NOT_SATISFIABLE = 416;
constexpr int HTTP_SERVER_ERR = 500;

} // namespace Response
