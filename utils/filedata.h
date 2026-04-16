#ifndef FILEDATA_H
#define FILEDATA_H
#include <QString>
#include <QJsonObject>
#include <QDateTime>


enum class FileType { File, Directory };

struct FileData
{
    qint64 id = 0;
    qint64 ownerId = 0;
    QString name;                   // Displayed name
    QString path;                   // Virtual path(user's path)
    QString serverPath;             // Path on server
    FileType type = FileType::File;
    qint64 sizeBytes = 0;
    QString mimeType;
    QString checkSum;               // SHA 256 of file content
    bool isShared = false;
    QString shareToken;
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastAccessed;

    bool isDirectory() const { return type == FileType::Directory; }

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["id"]           = id;
        obj["ownerId"]      = ownerId;
        obj["name"]         = name;
        obj["path"]         = path;
        obj["type"]         = isDirectory() ? "directory" : "file";
        obj["sizeBytes"]    = sizeBytes;
        obj["mimeType"]     = mimeType;
        obj["checksum"]     = checkSum;
        obj["isShared"]     = isShared;
        if (isShared) obj["shareToken"] = shareToken;
        obj["createdAt"]    = createdAt.toString(Qt::ISODate);
        obj["updatedAt"]    = updatedAt.toString(Qt::ISODate);
        obj["lastAccessed"] = lastAccessed.toString(Qt::ISODate);

        return obj;
    }
};

#endif // FILEDATA_H
