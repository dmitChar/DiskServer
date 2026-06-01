#ifndef STREAMINGSTRATEGY_H
#define STREAMINGSTRATEGY_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include "router.h"
#include "filedata.h"
#include <memory>


class StreamingStrategy : public QObject
{
    Q_OBJECT
    using QObject::QObject;
public:
    explicit StreamingStrategy(QObject *parent = nullptr);
    virtual ~StreamingStrategy() = default;
    virtual bool start(QTcpSocket *socket, const HttpResponse &resp);

protected slots:
    virtual void onBytesWritten(qint64 bytes);
    virtual void onSocketError(QAbstractSocket::SocketError error);
    virtual void onSocketDisconnected();
    virtual void onFinished(bool ok);

protected:
    QTcpSocket *m_socket;
    std::unique_ptr<QFile> m_file;
    StreamingInfo m_info;
    QMap<QString, QString> m_extraHeaders;  // Из resp.headers
    qint64 m_remaining = 0;                 // осталось отправить байт
    qint64 m_pending = 0;                   // записано в сокет байт
    bool m_done = false;
    const qint64 CHUNK_SIZE = 256 * 1024;


protected:
    virtual void sendHeaders() = 0;
    virtual void sendNextChunk() = 0;
    virtual void finilize();
    virtual void abort(const QString &reason);

    static QByteArray buildStatusLine(int code);

signals:
    void finished(bool success);
};

#endif // STREAMINGSTRATEGY_H
