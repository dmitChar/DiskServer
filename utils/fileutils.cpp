#include "fileutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QRegularExpression>

namespace FileUtils
{

    bool ensureDirectoryExists(const QString &path)
    {
        QDir dir;
        return dir.mkpath(path);
    }

    bool deleteFileOrDir(const QString &path)
    {
        QFileInfo info(path);
        if (!info.exists()) return true;
        if (info.isDir())
        {
            QDir dir(path);
            return dir.removeRecursively();
        }
        return QFile::remove(path);
    }

    qint64 dirSize(const QString &path)
    {
        qint64 size = 0;
        QDirIterator it(path, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isFile())
            {
                size += it.fileInfo().size();
            }
        }
        return size;
    }

    QString mimeTypeFromExtension(const QString &filename)
    {
        static const QMap<QString, QString> mimeMap = {
            {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"png", "image/png"},
            {"gif", "image/gif"},  {"bmp", "image/bmp"},   {"webp", "image/webp"},
            {"svg", "image/svg+xml"},
            {"pdf", "application/pdf"},
            {"txt", "text/plain"}, {"md", "text/markdown"}, {"csv", "text/csv"},
            {"html", "text/html"}, {"css", "text/css"},     {"js", "application/javascript"},
            {"json", "application/json"}, {"xml", "application/xml"},
            {"zip", "application/zip"}, {"tar", "application/x-tar"},
            {"gz", "application/gzip"}, {"7z", "application/x-7z-compressed"},
            {"rar", "application/vnd.rar"},
            {"mp3", "audio/mpeg"}, {"mp4", "video/mp4"}, {"avi", "video/x-msvideo"},
            {"mov", "video/quicktime"}, {"mkv", "video/x-matroska"},
            {"doc", "application/msword"},
            {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
            {"xls", "application/vnd.ms-excel"},
            {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
            {"ppt", "application/vnd.ms-powerpoint"},
            {"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
        };

        QString ext = QFileInfo(filename).suffix().toLower();
        return mimeMap.value(ext, "application/octet-stream");
    }

    QString sanitizePath(const QString &path)
    {
        QString clean = path;
        // Normalize separators
        clean.replace('\\', '/');
        // Remove double slashes
        static QRegularExpression doubleSlash("//+");
        clean.replace(doubleSlash, "/");
        // Remove leading slash
        if (clean.startsWith('/')) clean = clean.mid(1);
        return clean;
    }

    bool isPathTraversal(const QString &basePath, const QString &targetPath)
    {
        QFileInfo base(basePath);
        QFileInfo target(targetPath);
        QString canonical = target.canonicalFilePath();
        if (canonical.isEmpty()) {
            // File doesn't exist yet; check the absolute path
            canonical = target.absoluteFilePath();
        }
        return !canonical.startsWith(base.canonicalFilePath());
    }

    QString generateStoragePath(const QString &userId, const QString &relativePath)
    {
        return userId + "/" + sanitizePath(relativePath);
    }

    QStringList listDirectory(const QString &path)
    {
        QDir dir(path);
        if (!dir.exists()) return {};
        return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    }

} // namespace FileUtils
