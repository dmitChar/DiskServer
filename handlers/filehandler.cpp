#include "filehandler.h"
#include <QVector>
#include <QFileInfo>
#include <QFile>
#include <QDir>
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


// ─── Внутренний хелпер: выполнить чтение файла с учётом Range ────────────────
//
// Параметры:
//   meta        — метаданные файла из БД
//   rangeHeader — значение заголовка Range (может быть пустым)
//   outMime     — [out] MIME-тип для Content-Type
//   outFilename — [out] имя файла для Content-Disposition
//   outHeaders  — [out] дополнительные заголовки (Content-Range, Accept-Ranges)
//
// Возвращает {httpStatusCode, body}.
// body — либо весь файл (200), либо запрошенный фрагмент (206).
//
static QPair<int, QByteArray> serveFile(const FileData &fileData, const QString &rangeHeader, QString &outMime,
                                        QString &outFilename, QMap<QString, QString> outHeaders)
{
    outMime = fileData.mimeType;
    outFilename = fileData.name;

    outHeaders["Accept-Ranger"] = "bytes";
    QFile file(fileData.serverPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Failed to open file for reading")};
    }

    const qint64 fileSize = file.size();

    // Разбор Ranges заголовка
    auto rangeOpt = Response::parseRange(rangeHeader);

    if (!rangeOpt.has_value())
    {
        // if (fileData.mimeType.startsWith("video/"))
        //     return {Response::HTTP_PARTIAL_CONTENT, {}};

        // Нет Range заголовка - полный файл
        return {Response::HTTP_OK, file.readAll()};
    }

    Response::RangeRequest range = rangeOpt.value();
    if (!range.resolve(fileSize))
    {
        // Диапазон за пределами файла → 416 Range Not Satisfiable
        outHeaders["Content-Range"] = QString("bytes */%1").arg(fileSize);
        return {Response::HTTP_RANGE_NOT_SATISFIABLE, Response::error(416, "Range Not Satisfiable")};
    }

    // Чтение запрошенного фрагмента
    if (!file.seek(range.first))
    {
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Seek failed")};
    }

    QByteArray chunk = file.read(range.length());
    if (chunk.size() != range.length())
    {
        // Файл мог усечься между открытием и чтением
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Read returned fewer bytes than expected")};
    }

    // ── Заголовки для 206 Partial Content ─────────────────────────────────────
    // Content-Range: bytes <first>-<last>/<total>
    outHeaders["Content-Range"]  = range.contentRangeHeader(fileSize);
    // Content-Length: размер именно этого фрагмента
    outHeaders["Content-Length"] = QString::number(chunk.size());

    qDebug() << "[Range] Serving bytes"
             << range.first << "-" << range.last
             << "of" << fileSize
             << "for" << fileData.name;

    return {Response::HTTP_PARTIAL_CONTENT, chunk};

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
    if (!token)
        return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    QString normPath = "/" + FileUtils::sanitizePath(path.isEmpty() ? "/" : path);

    auto entries = m_db->listDirectory(token->userId, normPath);

    QJsonArray arr;
    for (const auto &f : entries)
        arr.append(f.toJson());

    return { Response::HTTP_OK, Response::successArray(arr) };
}

//--------- Создание новой папки ----------------

QPair<int, QByteArray> FileHandler::handleMkDir(const QString &authHeader, const QByteArray &body)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token)
        return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user)
        return { Response::HTTP_NOT_FOUND, Response::error(404, "User not found")};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "INVALID JSON")};

    QJsonObject obj = doc.object();
    QString clientPath = obj["path"].toString().trimmed();
    if (clientPath.isEmpty())
        return {Response::HTTP_BAD_REQ, Response::error(400, "Path is empty")};

    QString normPath = "/" + FileUtils::sanitizePath(clientPath);

    if (m_db->getFileByPath(token->userId, normPath))
        return {Response::HTTP_CONFLICT, Response::error(409, "Path already exists")};

    QString serverPath = diskPath(token->userId, FileUtils::sanitizePath(normPath.mid(1)));

    FileUtils::ensureDirectoryExists(serverPath);

    FileData data;
    data.ownerId = token->userId;
    data.name = QFileInfo(normPath).fileName();
    data.path = normPath;
    data.serverPath = serverPath;
    data.type = FileType::Directory;
    data.sizeBytes = 4096;

    auto created = m_db->createFile(data);
    if (!created)
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Failed to create a directory")};

    m_db->updateUserUsedBytes(data.ownerId, m_db->calcUsedBytes(data.ownerId));
    return {Response::HTTP_CREATED, Response::success(created->toJson(), "Directory created")};
}

//--------- Удаление файлов ----------------

QPair<int, QByteArray> FileHandler::handleDelete(const QString &authHeader,  qint64 fileId)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token) return { Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user) return { Response::HTTP_NOT_FOUND, Response::error(404, "User Not found") };

    auto file = m_db->getFileById(fileId);
    if (!file)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "File Not Found")};

    auto ownerId = m_db->getOwnerIdById(file.value().id);
    if (!ownerId)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "Owner Not Found")};

    if (ownerId != user.value().id)
        return { Response::HTTP_FORBIDDEN, Response::error(403, "You are not an owner of this file")};

    //QString filePath = FileUtils::sanitizePath(file.value().serverPath);
    const QString dPath = file.value().serverPath;
    bool success = false;
    if (file.value().isDirectory())
    {
        if (QDir(dPath).removeRecursively())
            success = m_db->deleteDirCascade(dPath);
    }
    else
    {
        if (QFile::remove(dPath))
            success = m_db->deleteFileById(file.value().id);
    }

    if (success)
    {
        qDebug() << "[FILEHANDLER] Был удален файл" << dPath;
        return {Response::HTTP_OK, Response::success({}, "File was deleted")};
    }
    else return {Response::HTTP_SERVER_ERR, Response::error(500, "File was not deleted")};
}

QPair<int, QByteArray> FileHandler::handleRenameFile(const QString &authHeader, const QString &filePath, const QByteArray &body)
{
    auto token = m_auth->authencticate(authHeader);
    if (!token)
        return {Response::HTTP_UNAUTH, Response::error(401, "Unauthorized")};

    auto user = m_db->getUserById(token->userId);
    if (!user) return { Response::HTTP_NOT_FOUND, Response::error(404, "User Not found") };

    auto oldFile = m_db->getFileByPath(filePath);
    if (!oldFile)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "File Not Found")};
    FileData newFile = oldFile.value();

    auto ownerId = m_db->getOwnerIdById(newFile.id);
    if (!ownerId)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "Owner Not Found")};

    if (ownerId != user.value().id)
        return { Response::HTTP_FORBIDDEN, Response::error(403, "You are not an owner of this file")};

    //--------------------
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {Response::HTTP_BAD_REQ, Response::error(400, "INVALID JSON")};

    QJsonObject obj = doc.object();
    QString newName = obj["newName"].toString().trimmed();
    if (newName.isEmpty())
        return {Response::HTTP_BAD_REQ, Response::error(400, "New name is empty")};

    QString dPath = diskPath(ownerId.value(), FileUtils::sanitizePath(filePath));
    bool success = false;


    QString temp = newFile.path;
    temp.chop(oldFile.value().name.size());

    QString newUserPath = QString(temp + newName);
    QString oldName = QFileInfo(dPath).fileName();

    temp = dPath;
    temp.chop(newFile.name.size());
    QString newServerPath = QString(temp + newName);

    newFile.name = newName;
    newFile.path = newUserPath;
    newFile.serverPath = newServerPath;


    // Обновление в бд и в системе
    if (m_db->updateFile(newFile))
        success = QFile::rename(dPath, newServerPath);
    else
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Error in changing file's name")};

    if (success)
    {
        if (newFile.isDirectory())
            m_db->updateDirCascade(oldName, newName);
        return {Response::HTTP_OK, Response::success({}, "Rename successful")};
    }
    else
        return {Response::HTTP_SERVER_ERR, Response::error(500, "Error in changing file's name")};
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
        QString absolutePath = QFileInfo(physPath).absolutePath();
        FileUtils::ensureDirectoryExists(absolutePath);
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

//--------- Загрузка файлов с сервера ----------------

QPair<int, QByteArray>FileHandler::handleDownload(const QString &authHeader, qint64 fileId, const QString &rangeHeader,
                                                  QString &outMime, QString &outFileName, QMap<QString, QString> &outHeaders)
{
    // auto token = m_auth->authencticate(authHeader);
    // if (!token)
    //     return { Response::HTTP_UNAUTH, Response::error(401, "Unauthorized") };

    if (fileId <= 0)
        return { Response::HTTP_BAD_REQ, Response::error(400, "Invalid Id") };

    auto file = m_db->getFileById(fileId);
    if (!file)
        return {Response::HTTP_NOT_FOUND, Response::error(404, "File not found")};

    if (file->isDirectory())
        return { Response::HTTP_BAD_REQ, Response::error(400, "Cannot download a directory")};

    if (rangeHeader.isEmpty())
    {
        file.value().lastAccessed = QDateTime::currentDateTime();
        m_db->updateFile(file.value());
    }

    return serveFile(file.value(), rangeHeader, outMime, outFileName, outHeaders);
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
