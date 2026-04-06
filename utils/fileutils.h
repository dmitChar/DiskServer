#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QObject>
#include <QStringList>

namespace FileUtils
{
    bool   ensureDirectoryExists(const QString &path);
    bool   deleteFileOrDir(const QString &path);
    qint64 dirSize(const QString &path);
    QString mimeTypeFromExtension(const QString &filename);
    QString sanitizePath(const QString &path);
    bool   isPathTraversal(const QString &basePath, const QString &targetPath);
    QString generateStoragePath(const QString &userId, const QString &relativePath);
    QStringList listDirectory(const QString &path);
}


#endif // FILEUTILS_H
