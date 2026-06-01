#include "StreamingStrategy.h"

StreamingStrategy::StreamingStrategy(QObject *parent)
    :QObject(parent)
{

}

bool StreamingStrategy::start(QTcpSocket *socket, const HttpResponse &resp)
{
    m_info = resp.streamInfo;
    m_socket = socket;
    m_file = std::make_unique<QFile>(m_info.filePath);
    m_extraHeaders = resp.headers;
    m_socket->setParent(this);

    connect(this, &StreamingStrategy::finished, this, &StreamingStrategy::onFinished);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &StreamingStrategy::onBytesWritten);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &StreamingStrategy::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &StreamingStrategy::onSocketDisconnected);

    if (!m_file.get()->open(QIODevice::ReadOnly))
    {
        qDebug() << "[" << metaObject()->className() << "] Cannot open file: " << m_info.filePath;
        abort("File open failed");
        return false;
    }

    const qint64 fileSize = m_file.get()->size();

    // Диапазон для отправки
    qint64 offset = m_info.offset;
    qint64 length = m_info.length;

    if (length < 0 || length > fileSize - offset)
        length = fileSize - offset;

    if (offset > 0 && !m_file.get()->seek(offset))
    {
        qDebug() << "[" << metaObject()->className() << "] Seek failed to" << offset;
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

void StreamingStrategy::onBytesWritten(qint64 bytes)
{
    m_pending -= bytes;
    // Когда qt буфер опустел - можно отправить следующий кусок
    if (m_pending <= 0)
    {
        m_pending = 0;
        sendNextChunk();
    }
}

void StreamingStrategy::onSocketError(QAbstractSocket::SocketError error)
{
    if (m_done)
        return;
    qDebug() << "[" << metaObject()->className() << "] Socket error:" << m_socket->errorString();
    abort(m_socket->errorString());
}

void StreamingStrategy::onSocketDisconnected()
{
    if (m_done)
        return;
    qDebug() << "[" << metaObject()->className() << "] Client disconnected";
    m_done = true;
    if (m_file.get()->isOpen())
        m_file.get()->close();
    emit finished(false);
}

void StreamingStrategy::onFinished(bool ok)
{
    qDebug() << "[" <<  metaObject()->className() << "] Finished" << m_info.filePath << (ok ? "OK" : "FAILED") << "socket:" << m_socket;
}


void StreamingStrategy::finilize()
{
    if (m_done)
        return;
    m_done = true;
    m_file.get()->close();

    if (m_info.useChunkedTE && !m_info.isRange)
    {
        // Терминирующий чанк
        m_socket->write("0\r\n\r\n");
    }
    m_socket->flush();
    m_socket->disconnectFromHost();
    qDebug() << "[" << metaObject()->className() << "] Done:" << m_info.filePath;
    emit finished(true);
}

void StreamingStrategy::abort(const QString &reason)
{
    if (m_done)
        return;
    m_done = true;
    if (m_file.get()->isOpen())
        m_file.get()->close();

    qDebug() << "[" << metaObject()->className() << "] Aborted! Reason:" << reason;

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
}

QByteArray StreamingStrategy::buildStatusLine(int code)
{
    static const QMap<int, QByteArray> texts =
    {
        {200, "OK"}, {206, "Partial Content"}, {500, "Interval Server Error"}
    };

    return "HTTP/1.1 " + QByteArray::number(code) + " " + texts.value(code, "Unknown") + "\r\n";
}
