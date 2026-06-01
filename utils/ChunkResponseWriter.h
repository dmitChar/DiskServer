#ifndef CHUNKRESPONSEWRITER_H
#define CHUNKRESPONSEWRITER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include "router.h"

static constexpr qint64 CHUNK_SIZE = 256 * 1024; // 256  KB
class ChunkResponseWriter : public QObject
{
    Q_OBJECT
public:
    explicit ChunkResponseWriter(QTcpSocket* socket, const HttpResponse &resp, QObject *parent = nullptr);
    ~ChunkResponseWriter();

    // Начать стриминг. Возвращает false если файл не открылся
    bool start();

private slots:
    void onBytesWritten(qint64 bytes);
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketDisconnected();

private:
    void sendHeaders();
    void sendNextChunk();
    void finilize();
    void abort(const QString &reason);

    static QByteArray buildStatusLine(int code);

    QTcpSocket *m_socket;
    QFile m_file;
    StreamingInfo m_info;
    QMap<QString, QString> m_extraHeaders; // Из resp.headers
    qint64 m_remaining = 0; // осталось отправить байт
    qint64 m_pending = 0; // записано в сокет байт
    bool m_done = false;

signals:
    void finished(bool success);
};

#endif // CHUNKRESPONSEWRITER_H
