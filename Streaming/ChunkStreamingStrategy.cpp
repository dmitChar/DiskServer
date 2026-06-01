#include "ChunkStreamingStrategy.h"

ChunkStreamingStrategy::ChunkStreamingStrategy(QObject *parent)
    : StreamingStrategy (parent)
{
}

ChunkStreamingStrategy::~ChunkStreamingStrategy()
{
    if (m_file.get()->isOpen())
        m_file.get()->close();
}

void ChunkStreamingStrategy::sendHeaders()
{
    const int code = m_info.isRange ? 206 : 200;
    QByteArray raw;
    raw += buildStatusLine(code);
    raw += "Date: " + QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ssZ").toLatin1() + " GMT\r\n";
    raw += "Server: CloudDisk-Backend/1.0\r\n";
    raw += "Connection: close\r\n";

    //Копия заголовков из HttpResponse
    for (auto it = m_extraHeaders.constBegin(); it != m_extraHeaders.constEnd(); ++it)
    {
        QString key = it.key().toLower();
        if (key == "content-length" || key == "transfer-encoding")
            continue;
        raw += it.key().toLatin1() + ": " + it.value().toLatin1() + "\r\n";
    }

    // Полный файл -> Transfer-Encoding: chuncked (content-length не нужен)
    raw += "Tranfer-Encoding: chunked\r\n";

    m_pending += raw.size();
    m_socket->write(raw);

    qDebug() << "[" << metaObject()->className() << "] Headers sent" << code << "file:" << m_info.filePath << "range:" << m_info.isRange << "length:" << m_info.length;
}

void ChunkStreamingStrategy::sendNextChunk()
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
    const QByteArray data = m_file.get()->read(toRead);

    if (data.isEmpty())
    {
        qDebug() << "[" << metaObject()->className() << "] Unexpected EOF, remaining =" << m_remaining;
        finilize();
        return;
    }
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

    ++flag;
    if (flag % 50 == 0 || flag == 1)
        qDebug() << "[" << metaObject()->className() << "] Chunk" << data.size() << "bytes remaining =" << m_remaining;
}
