#include "RangeStreamingStrategy.h"

RangeStreamingStrategy::RangeStreamingStrategy(QObject *parent)
    : StreamingStrategy (parent)
{
}

RangeStreamingStrategy::~RangeStreamingStrategy()
{
    if (m_file.get()->isOpen())
        m_file.get()->close();
}

void RangeStreamingStrategy::sendHeaders()
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

    // Range-ответ -> Content-Length = точный размер
    raw += "Content-Range: bytes " + QByteArray::number(m_info.offset) + "-" + QByteArray::number(m_info.offset + m_info.length - 1) + "/" + QByteArray::number(m_info.fileSize) + "\r\n";
    raw += "Content-Length: " + QByteArray::number(m_info.length) + "\r\n";
    raw += "\r\n";

    m_pending += raw.size();
    m_socket->write(raw);

    qDebug() << "[" << metaObject()->className() << "] Headers sent" << code << "file:" << m_info.filePath << "range:" << m_info.isRange << "length:" << m_info.length;
}

void RangeStreamingStrategy::sendNextChunk()
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
    m_remaining -= data.size();
    m_pending += data.size();
    m_socket->write(data);

    ++flag;
    if (flag % 50 == 0 || flag == 1)
        qDebug() << "[" << metaObject()->className() << "] Chunk" << data.size() << "bytes remaining =" << m_remaining;
}
