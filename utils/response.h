#pragma once
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <optional>

namespace Response {

// ── JSON helpers ──────────────────────────────────────────────────────────────

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
    resp["data"]    = data;
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

inline QByteArray error(int code, const QString &message) {
    QJsonObject resp;
    resp["success"] = false;
    resp["error"]   = QJsonObject{{"code", code}, {"message", message}};
    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

// ── HTTP status codes ─────────────────────────────────────────────────────────

constexpr int HTTP_OK                   = 200;
constexpr int HTTP_PARTIAL_CONTENT      = 206;   // ← Range ответ
constexpr int HTTP_CREATED              = 201;
constexpr int HTTP_NO_CONTENT           = 204;
constexpr int HTTP_BAD_REQ              = 400;
constexpr int HTTP_UNAUTH               = 401;
constexpr int HTTP_FORBIDDEN            = 403;
constexpr int HTTP_NOT_FOUND            = 404;
constexpr int HTTP_CONFLICT             = 409;
constexpr int HTTP_TOO_LARGE            = 413;
constexpr int HTTP_RANGE_NOT_SATISFIABLE = 416;  // ← Range за пределами файла
constexpr int HTTP_SERVER_ERR           = 500;

// ── Range request parsing ────────────────────────────────────────────────────

// Результат разбора заголовка "Range: bytes=<first>-<last>"
struct RangeRequest
{
    qint64 first = 0;      // включительно
    qint64 last  = -1;     // включительно; -1 = до конца файла

    // Вычислить last относительно реального размера файла
    // После вызова: first и last — оба валидные индексы байт
    bool resolve(qint64 fileSize)
    {
        if (fileSize <= 0) return false;

        // suffix form: bytes=-N → последние N байт
        if (first < 0) {
            first = fileSize + first;   // first хранит -N
            last  = fileSize - 1;
        } else if (last < 0 || last >= fileSize) {
            last = fileSize - 1;        // открытый диапазон или за пределами
        }

        return first >= 0 && first <= last && last < fileSize;
    }

    qint64 length() const { return last - first + 1; }

    // Строка для заголовка Content-Range: bytes first-last/total
    QString contentRangeHeader(qint64 fileSize) const
    {
        return QString("bytes %1-%2/%3").arg(first).arg(last).arg(fileSize);
    }
};

// Разбирает заголовок Range: bytes=<spec>
// Поддерживает:
//   bytes=0-1023     → {0, 1023}
//   bytes=1024-      → {1024, -1}  (до конца)
//   bytes=-512       → {-512, -1}  (суффикс: последние 512)
//
// Возвращает std::nullopt если заголовок отсутствует, пустой или неверного формата.
inline std::optional<RangeRequest> parseRange(const QString &rangeHeader)
{
    // Отсутствует — обычный запрос без Range
    if (rangeHeader.isEmpty()) return std::nullopt;

    // Должен начинаться с "bytes="
    if (!rangeHeader.startsWith("bytes=", Qt::CaseInsensitive))
        return std::nullopt;

    QString spec = rangeHeader.mid(6).trimmed();  // убираем "bytes="

    if (spec.contains(',')) return std::nullopt;

    RangeRequest req;

    if (spec.startsWith('-'))
    {
        // Suffix form: bytes=-N
        bool ok = false;
        qint64 n = spec.toLongLong(&ok);
        if (!ok || n >= 0) return std::nullopt;
        req.first = n;
        req.last  = -1;
    }
    else
    {
        int dash = spec.indexOf('-');
        if (dash < 0) return std::nullopt;

        QString firstStr = spec.left(dash).trimmed();
        QString lastStr  = spec.mid(dash + 1).trimmed();

        bool ok = false;
        req.first = firstStr.toLongLong(&ok);
        if (!ok || req.first < 0) return std::nullopt;

        if (lastStr.isEmpty())
        {
            req.last = -1;   // открытый диапазон
        }
        else
        {
            req.last = lastStr.toLongLong(&ok);
            if (!ok || req.last < req.first) return std::nullopt;
        }
    }

    return req;
}

} // namespace Response
