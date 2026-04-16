#pragma once
#include <QString>
#include <QJsonObject>
#include <QDateTime>

struct User
{
    qint64  id        = 0;
    QString username;
    QString email;
    QString passwordHash;
    QString salt;
    qint64  quotaBytes  = 0;   // Total quota in bytes
    qint64  usedBytes   = 0;   // Used storage in bytes
    bool    isActive    = true;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isValid() const
    {
        return id > 0 && !username.isEmpty();
    }

    QJsonObject toJson(bool includeSensitive = false) const
    {
        QJsonObject obj;
        obj["id"]          = id;
        obj["username"]    = username;
        obj["email"]       = email;
        obj["quotaBytes"]  = quotaBytes;
        obj["usedBytes"]   = usedBytes;
        obj["freeBytes"]   = quotaBytes - usedBytes;
        obj["isActive"]    = isActive;
        obj["createdAt"]   = createdAt.toString(Qt::ISODate);
        if (includeSensitive) {
            obj["passwordHash"] = passwordHash;
            obj["salt"]         = salt;
        }
        return obj;
    }
};
