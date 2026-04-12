#include "filehandler.h"
#include <QVector>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QDebug>

FileHandler::FileHandler(DatabaseManager *db, AuthMiddleware *auth, const QString &storageRoot, qint64 maxFileSizeBytes, QObject *parent)
    : QObject(parent), m_db(db), m_auth(auth), m_storageRoot(storageRoot), m_maxFileSizeBytes(maxFileSizeBytes)
{

}

//--------- Utils ----------------

QString FileHandler::diskPath(qint64 userId, const QString &relativePath) const
{
    return m_storageRoot + "/" + QString::number(userId) + "/" + relativePath;
}

// Обновление квоты пользователя в бд
void FileHandler::refreshUsedBytes(qint64 userId)
{
    qint64 used = m_db->calcUsedBytes(userId);
    m_db->updateUserUsedBytes(userId, used);
}

//--------- Получение списка файлов и папок в директории ----------------

QPair<int, QByteArray> FileHandler::handleList(const QString &authHeader, const QString &path)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    QString normPath = "/" + FileUtils::sanitizePath(path.isEmpty() ? "/" : path);

    auto entries = m_db->listDirectory(token->userId, normPath);

    QJsonArray arr;
    for (const auto &f : entries)
        arr.append(f.toJson());

    return { Response::HTTP_OK, Response::successArray(arr) };
}

//--------- Загрузка файлов на сервер ----------------

QPair<int, QByteArray> FileHandler::handleUpload(const QString &authHeader, const QString &targetDir, const QByteArray &body, const QString &contentType)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user) return { Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    // Извлечение boundary
    QString boundary;
    for (const QString &part : contentType.split(';'))
    {
        QString p = part.trimmed();
        if (p.startsWith("boundary="))
        {
            boundary = p.mid(9).remove('"');
            break;
        }
    }
    if (boundary.isEmpty())
        return { Response::HTTP_BAD_REQ, Response::error(400, "Missing multipart boundary") };

    QList<MultipartFile> files = parseMultipart(body, boundary);
    if (files.isEmpty())
        return { Response::HTTP_BAD_REQ, Response::error(400, "No files found in request") };

    QJsonArray uploaded;
    for (const auto &mf : files)
    {
        if (mf.fileName.isEmpty() || mf.data.isEmpty())
            continue;

        // Size check
        if (mf.data.size() > m_maxFileSizeBytes)
            return {Response::HTTP_TOO_LARGE, Response::error(413, QString("File '%1' exceeds size limit").arg(mf.fileName ))};

        // Quota check
        qint64 freeBytes = user->quotaBytes - m_db->calcUsedBytes(token->userId);
        if (mf.data.size() > freeBytes )
        {
            return { Response::HTTP_FORBIDDEN, Response::error(403, "Storage quota exceeded") };
        }

        QString dir = "/" + FileUtils::sanitizePath(targetDir.isEmpty() ? "/" : targetDir);
        QString safeFileName = QFileInfo(mf.fileName).fileName();
        QString virtualPath = dir.endsWith('/') ? dir + safeFileName : dir + "/" + safeFileName;
        QString physPath = diskPath(token->userId, FileUtils::sanitizePath(virtualPath.mid(1)));

        // Write to disk
        FileUtils::ensureDirectoryExists(QFileInfo(physPath).absoluteFilePath());
        QFile f(physPath);
        if (!f.open(QIODevice::WriteOnly))
        {
            qDebug() << "[File] Cannot write:" << physPath;
            continue;
        }
        f.write(mf.data);
        f.close();

        // Checksum
        QString checksum = QCryptographicHash::hash(mf.data, QCryptographicHash::Sha256).toHex();


        auto existing = m_db->getFileByPath(token->userId, virtualPath);

        if (existing) // Обновление существующего файла в бд
        {
            existing->sizeBytes = mf.data.size();
            existing->checkSum = checksum;
            existing->mimeType = FileUtils::mimeTypeFromExtension(safeFileName);
            existing->serverPath = physPath;
            m_db->updateFile(*existing);

            uploaded.append(existing->toJson());
        }
        else        // Добавление нового файла в бд
        {
            FileData data;
            data.ownerId = token->userId;
            data.name = safeFileName;
            data.path = virtualPath;
            data.serverPath = physPath;
            data.type = FileType::File;
            data.sizeBytes = mf.data.size();
            data.mimeType = FileUtils::mimeTypeFromExtension(safeFileName);
            data.checkSum = checksum;
            auto created = m_db->createFile(data);

            if (created)
                uploaded.append(created->toJson());
        }
    }
    refreshUsedBytes(token->userId);
    return {Response::HTTP_CREATED, Response::successArray(uploaded, "Upload successful")};
}

QPair<int, QByteArray> FileHandler::handleDownload(const QString &authHeader, const QString &filePath, QString &outMime, QString &outFileName)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token)
        return { Response::HTTP_UNAUTH, Response::error(401, "Unauthorized") };

    QString normPath = "/" + FileUtils::sanitizePath(filePath);
    auto file = m_db->getFileByPath(token->userId, normPath);
    if (!file)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "File not found")};
    if (file->isDirectory())
        return { Response::HTTP_BAD_REQ, Response::error(400, "Cannot download a directory")};

    QFile f(file->serverPath);
    if (!f.open(QIODevice::ReadOnly))
        return { Response::HTTP_SERVER_ERR, Response::error(500, "Failed to read file")};

    outMime = file->mimeType;
    outFileName = file->name;

    // Update last accessed
    file->lastAccessed = QDateTime::currentDateTimeUtc();
    m_db->updateFile(*file);

    return {Response::HTTP_OK, f.readAll()};
}


/**
 * @brief FileHandler::parseMultipart
 * @param body Тело запроса. Содержит в себе boundary + сами данные: ------Qt-boundary\r\n
 *                                                                   Content-Disposition: form-data; name="file"; filename="cat.jpg"\r\n
 *                                                                   Content-Type: image/jpeg\r\n
 *                                                                   \r\n
 *                                                                   <байты cat.jpg>
 *                                                                   \r\n------Qt-boundary--\r\n
 * @param boundary boundary запроса
 * @return MultipartFile, который содержит:  filename="cat.jpg"
 *                                           contentType="image/jpeg"
 *                                           data="<байты jpg>"
 */
QList<MultipartFile> FileHandler::parseMultipart(const QByteArray &body, const QString &boundary)
{
    // Инициализация делителей
    QList<MultipartFile> result;
    QByteArray delimiter = "--" + boundary.toUtf8();
    QByteArray endDelimiter = delimiter + "--";

    int pos = 0;
    while (pos < body.size())
    {
        // Ищем начало нового boundary
        int start = body.indexOf(delimiter, pos);
        if (start < 0)
            break;
        start += delimiter.size(); // Указывает на символы после boundary

        // Указываем, является найденный boundary началом либо концом
        if (body.mid(start, 2) == "\r\n")
            start += 2;                     // Если начало, то пропускаем 2 символа /r/n
        else if(body.mid(start, 2) == "--") // Если конец то выходим
            break;

        int end = body.indexOf("\r\n" + delimiter, start);
        if (end < 0)
            break;


        QByteArray part = body.mid(start, end - start); // Все данные от начала boundary до конца этого boundary

        // Отделелние заголовков от байтов данных
        int headerEnd = part.indexOf("\r\n\r\n"); // Ищем где заканчиваются заголовки
        if (headerEnd < 0)
        {
            pos = end + 2;
            continue;
        }

        QByteArray headers = part.left(headerEnd); // Заголовки
        QByteArray data = part.mid(headerEnd + 4); // Данные

        MultipartFile mf;
        mf.data = data;

        for (const QByteArray &hdr : headers.split('\n'))
        {
            QString line = QString::fromUtf8(hdr).trimmed();

            // Считываем данные из заголовка Content-Disposition
            if (line.startsWith("Content-Disposition:", Qt::CaseInsensitive))
            {
                for (const QString &seg : line.split(';'))
                {
                    QString s = seg.trimmed();
                    if (s.startsWith("name="))
                        mf.fieldName = s.mid(5).remove('"');
                    else if (s.startsWith("filename="))
                        mf.fileName = s.mid(9).remove('"');
                }
            }
            else if (line.startsWith("Content-Type:"), Qt::CaseInsensitive)
            {
                mf.contentType = line.mid(13).trimmed();
            }
        }

        if (!mf.fileName.isEmpty())
            result.append(mf);

        pos = end + 2; // Перепрыгнуть \r\n перед следующим boundary
    }
    return result;
}
