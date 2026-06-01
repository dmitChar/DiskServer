#include "ChunkResponseWriter.h"
#include <QDateTime>

ChunkResponseWriter::ChunkResponseWriter(QTcpSocket *socket, const HttpResponse &resp, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_file(resp.streamInfo.filePath)
    , m_extraHeaders(resp.headers)
{
    // Перенос владение сокетом на этот объект
    m_socket->setParent(this);
    m_info = resp.streamInfo;

    connect(m_socket, &QTcpSocket::bytesWritten, this, &ChunkResponseWriter::onBytesWritten);
//    connect(m_socket, &QAbstractSocket::errorOccured, this, &ChunkResponseWriter::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChunkResponseWriter::onSocketDisconnected);
}

ChunkResponseWriter::~ChunkResponseWriter()
{
    if (m_file.isOpen())
        m_file.close();
}

bool ChunkResponseWriter::start()
{
    if (!m_file.open(QIODevice::ReadOnly))
    {
        qDebug() << "[Stream] Cannot open file: " << m_info.filePath;
        abort("File open failed");
        return false;
    }

    const qint64 fileSize = m_file.size();

    // Диапазон для отправки
    qint64 offset = m_info.offset;
    qint64 length = m_info.length;

    if (length < 0 || length > fileSize - offset)
        length = fileSize - offset;

    if (offset > 0 && !m_file.seek(offset))
    {
        qDebug() << "[Stream] Seek failed to" << offset;
        abort("Seek failed");
        return false;
    }

    m_remaining = length;
    m_info.length = length;
    m_info.fileSize = fileSize;

    sendHeaders();
    sendNextChunk();
    return true;
}

void ChunkResponseWriter::sendHeaders()
{
    const int code = m_info.isRange ? 206 : 200;
    QByteArray raw;
    raw += buildStatusLine(code);
    raw += "Date: " + QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toLatin1() + " GMT\r\n";
    raw += "Server: CloudDisk-Backend/1.0\r\n";
    raw += "Accept-Ranges: bytes\r\n";
    raw += "Connection: close\r\n";

    //Копия заголовков из HttpResponse
    for (auto it = m_extraHeaders.constBegin(); it != m_extraHeaders.constEnd(); ++it)
    {
        QString key = it.key().toLower();
        if (key == "content-length" || key == "transfer-encoding")
            continue;
        raw += it.key().toLatin1() + ": " + it.value().toLatin1() + "\r\n";
    }

    if (m_info.isRange)
    {
        // Range-ответ -> Content-Length = точный размер
        raw += "Content-Range: bytes " + QByteArray::number(m_info.offset) + "-" + QByteArray::number(m_info.offset + m_info.length - 1) + "/"
                + QByteArray::number(m_info.fileSize) + "\r\n";
        raw += "Content-Length: " + QByteArray::number(m_info.length) + "\r\n";
    }
    else if (m_info.useChunkedTE)// Полный файл -> Transfer-Encoding: chuncked (content-length не нужен)
    {
        raw += "Tranfer-Encoding: chunked\r\n";
    }
    else
    {
        raw += "Content-Length: " + QByteArray::number(m_info.length) + "\r\n";
    }
    raw += "\r\n";

    m_pending += raw.size();
    m_socket->write(raw);

    qDebug() << "[Stream] Headers sent" <<code << "file:" << m_info.filePath << "range:" << m_info.isRange << "length:" << m_info.length;

}

void ChunkResponseWriter::sendNextChunk()
{
    static size_t flag = 0;
    if (m_done)
        return;
    if (m_remaining == 0)
    {
        finilize();
        return;
    }

    // Чтение следующего куска с диска
    const qint64 toRead = qMin(CHUNK_SIZE, m_remaining);
    const QByteArray data = m_file.read(toRead);

    if (data.isEmpty())
    {
        qWarning() << "[Stream] Unexpected EOF, remaining =" << m_remaining;
        finilize();
        return;
    }
    m_remaining -= data.size();

    if (m_info.useChunkedTE && !m_info.isRange)
    {
        // Chuncked TE формат
        // SIZE_IN_HEX\r\n
        // DATA\r\n
        QByteArray wire;
        wire.reserve(data.size() + 20);
        wire += QByteArray::number(data.size(), 16).toUpper();
        wire += "\r\n";
        wire += data;
        wire += "\r\n";

        m_pending += wire.size();
        m_socket->write(wire);
    }
    else
    {
        //  Range или отключенный chunked TE
        m_pending += data.size();
        m_socket->write(data);
    }
    ++flag;
    if (flag % 50 == 0)
        qDebug() << "[Stream] Chunk" << data.size() << "bytes remaining =" << m_remaining;
}

// Слот вызывается каждый раз, когда очередная порция данных была передана из qt буфера в буфер сокета
void ChunkResponseWriter::onBytesWritten(qint64 bytes)
{
    m_pending -= bytes;
    // Когда qt буфер опустел - можно отправить следующий кусок
    if (m_pending <= 0)
    {
        m_pending = 0;
        sendNextChunk();
    }
}

void ChunkResponseWriter::onSocketError(QAbstractSocket::SocketError error)
{
    if (m_done)
        return;
    qDebug() << "[Stream] Socket error:" << m_socket->errorString();
    abort(m_socket->errorString());
}

void ChunkResponseWriter::onSocketDisconnected()
{
    if (m_done)
        return;
    qDebug() << "[Stream] Client disconnected";
    m_done = true;
    if (m_file.isOpen())
        m_file.close();
    emit finished(false);
    deleteLater();
}

void ChunkResponseWriter::finilize()
{
    if (m_done)
        return;
    m_done = true;
    m_file.close();

    if (m_info.useChunkedTE && !m_info.isRange)
    {
        // Терминирующий чанк
        m_socket->write("0\r\n\r\n");
    }
    m_socket->flush();
    m_socket->disconnectFromHost();
    qDebug() << "[Stram] Done:" << m_info.filePath;
    emit finished(true);
    deleteLater();
}

void ChunkResponseWriter::abort(const QString &reason)
{
    if (m_done)
        return;
    m_done = true;
    if (m_file.isOpen())
        m_file.close();

    qDebug() << "[Stream] Aborted! Reason:" << reason;

    // Попытка отправить 500 если заголовки еще не ушли
    if (m_pending == 0)
    {
        QByteArray error = "HTTP/1.1 500 Internal Server Error\r\n";
        error += "Content-Length: 0\r\n";
        error += "Connection: close\r\n";
        m_socket->write(error);
        m_socket->flush();
    }
    m_socket->disconnectFromHost();
    emit finished(false);
    deleteLater();
}

QByteArray ChunkResponseWriter::buildStatusLine(int code)
{
    static const QMap<int, QByteArray> texts =
    {
        {200, "OK"}, {206, "Partial Content"}, {500, "Interval Server Error"}
    };
    return "HTTP/1.1 " + QByteArray::number(code) + " " + texts.value(code, "Unknown") + "\r\n";
}



