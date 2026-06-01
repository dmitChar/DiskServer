#ifndef HTTPSSERVER_H
#define HTTPSSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"
#include "handlers/authhandler.h"
#include "handlers/filehandler.h"
#include "handlers/userhandler.h"
#include "router.h"
#include "utils/ChunkResponseWriter.h"
#include "Streaming/ChunkStreamingStrategy.h"
#include "Streaming/StreamingStrategy.h"
#include "Streaming/RangeStreamingStrategy.h"
#include "Streaming/FullStreamingStrategy.h"
#include "Streaming/StreamingContext.h"

class HttpsServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpsServer(QObject *parent = nullptr);
public:
    bool start(const QString &host, quint16 port);
    void stop();
    void configure(DatabaseManager *db, AuthMiddleware *auth, AuthHandler *authH, FileHandler *fileH, UserHandler *userH);

private:
    void setUpRoutes();
    void processRequest(QTcpSocket *socket, const QByteArray &rawData);
    HttpRequest parseRequest(const QByteArray &rawData);
    void sendResponse(QTcpSocket *socket, const HttpResponse &resp);
    QString statusText(int code);
    void sendStreamingResponse(QTcpSocket *socket, const HttpResponse &resp);

private:
    DatabaseManager *m_db;
    QTcpServer *m_tcpServer;
    Router m_router;
    QMap<QTcpSocket*, QByteArray> m_buffers;

    AuthHandler *m_authHandler = nullptr;
    FileHandler *m_fileHandler = nullptr;
    UserHandler *m_userHandler = nullptr;
    StreamingContext m_streamingContext;

private slots:
    void onReadyRead();
    void onNewConnection();
    void onDisconnected();

};

class CustomTcpServer : public QTcpServer
{
protected:
    void incomingConnection(qintptr) override;
};

#endif // HTTPSSERVER_H
