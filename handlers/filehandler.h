#ifndef FILEHANDLER_H
#define FILEHANDLER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"
#include "utils/fileutils.h"
#include "utils/response.h"
#include "utils/filedata.h"
#include "router.h"

struct MultipartFile
{
    QString fieldName;
    QString fileName;
    QString contentType;
    QByteArray data;
};

class FileHandler : public QObject
{
    Q_OBJECT
public:
    explicit FileHandler(DatabaseManager *db, AuthMiddleware *auth, const QString &storageRoot,
                         qint64 maxFileSizeBytes, QObject *parent = nullptr);

    // GET  /api/files?path=/some/dir
    QPair<int, QByteArray> handleList(const QString &authHeader, const QString &path);

    // POST /api/files/upload?path=/some/dir
    QPair<int, QByteArray> handleUpload(const QString &authHeader, const QString &targetDir, const QByteArray &body, const QString &contentType);

    // GET /api/files/:id/download
    // rangeHeader — значение заголовка "Range" (пустая строка = полный файл)
    // outHeaders  — дополнительные заголовки ответа (Content-Range, Accept-Ranges)
    QPair<int, QByteArray> handleDownload(const QString &authHeader, qint64 fileId, const QString &rangeHeader,
                                          QString &outMime, QString &outFileName, QMap<QString, QString> &outHeaders);

    QPair<int, QByteArray> handleDownloadByPath(const QString &authHeader, const QString &filePath, const QString &rangeHeader,
                                          QString &outMime, QString &outFileName, QMap<QString, QString> &outHeaders);

    // POST /api/files/mkdir
    QPair<int, QByteArray> handleMkDir(const QString &authHeader, const QByteArray &body);

    QPair<int, QByteArray> handleDelete(const QString &authHeader, qint64 fileId);

    QPair<int, QByteArray> handleRenameFile(const QString &authHeader, const QString &filePath, const QByteArray &body);

    HttpResponse handleDownloadStream(const HttpRequest &req, qint64 fileId);


private:
    QList<MultipartFile> parseMultipart(const QByteArray &body, const QString &boundary);
    QString diskPath(qint64 userId, const QString &relativePath) const;
    void refreshUsedBytes(qint64 userId);


private:
    AuthMiddleware *m_auth;
    DatabaseManager *m_db;
    QString m_storageRoot;
    quint64 m_maxFileSizeBytes;

};

#endif // FILEHANDLER_H
