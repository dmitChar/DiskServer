#include "httpsserver.h"

#include <QUrlQuery>
// #include "utils/hashutils.h"


HttpsServer::HttpsServer(QObject *parent)
    : QObject{parent}, m_tcpServer(new QTcpServer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &HttpsServer::onNewConnection);
    // QString password = "12345678";
    // QString salt = HashUtils::generateSalt();
    // QString hash = HashUtils::hashPassword(password, salt);
    // m_db->createUser("admin", "admin@gmail.com", hash, salt, 100000 );
}

bool HttpsServer::start(const QString &host, quint16 port)
{
    QHostAddress addr = (host == "0.0.0.0") ? QHostAddress::Any : QHostAddress(host);
    if (!m_tcpServer->listen(addr, port))
    {
        qCritical() << "[HTTP Server] Failed to listen:" << m_tcpServer->errorString();
        return false;
    }
    qInfo() << "[HTTP Server] Listening on" << host << ":" <<port;
    return true;
}

void HttpsServer::stop()
{
    if (m_tcpServer->isListening())
        m_tcpServer->close();
}

void HttpsServer::configure(DatabaseManager *db, AuthMiddleware *auth,
                            AuthHandler *authH, FileHandler *fileH, UserHandler *userH)
{
    m_authHandler = authH;
    m_fileHandler = fileH;
    m_userHandler = userH;

    setUpRoutes();
}

// -------- ROUTES --------------------------------------------------

void HttpsServer::setUpRoutes()
{
    //----------- Auth -----------
    m_router.addRoute("POST", "/api/auth/register", [this](const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_authHandler->handleRegister(r.body);
        int statusCode = data.first;
        QByteArray body = data.second;

        HttpResponse resp;
        resp.statusCode = statusCode;
        resp.setJson();
        resp.body = body;
        return resp;
    });

    m_router.addRoute("POST", "/api/auth/login", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_authHandler->handleLogin(r.body);
        int statusCode = data.first;
        QByteArray body = data.second;

        HttpResponse resp;
        resp.statusCode = statusCode;
        resp.setJson();
        resp.body = body;
        return resp;
    });

    m_router.addRoute("POST", "/api/auth/logout", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_authHandler->handleLogout(r.getHeaderData("authorization"));
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();
        resp.body = data.second;
        return resp;
    });

    m_router.addRoute("GET", "/api/auth/me", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_authHandler->handleMe(r.getHeaderData("authorization"));
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();;
        resp.body = data.second;
        return resp;
    });

    m_router.addRoute("PUT", "/api/auth/password", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int,QByteArray> data = m_authHandler->handleChangePassword(r.getHeaderData("authorization"), r.body);
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();
        resp.body = data.second;
        return resp;
    });


    //----------- User -----------
    m_router.addRoute("GET", "/api/user/profile", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_userHandler->handleGetProfile(r.getHeaderData("authorization"));
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();
        resp.body = data.second;

        return resp;
    });

    m_router.addRoute("GET", "/api/user/quota", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_userHandler->handleGetQuota(r.getHeaderData("authorization"));
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();
        resp.body = data.second;

        return resp;
    });

    // Обновление профиля
    m_router.addRoute("PUT", "/api/user/profile", [this] (const HttpRequest &r, const QMap<QString, QString>&)
    {
        QPair<int, QByteArray> data = m_userHandler->handleUpdateProfile(r.getHeaderData("authorization"), r.body);
        HttpResponse resp;
        resp.statusCode = data.first;
        resp.setJson();
        resp.body = data.second;

        return resp;
    });
}

//-------- Request parsing -------------------------------------------

void HttpsServer::processRequest(QTcpSocket *socket, const QByteArray &rawData)
{
    HttpRequest req = parseRequest(rawData);

    // CORS preflight
    if (req.method == "OPTIONS")
    {
        HttpResponse resp;
        resp.statusCode = 204;
        resp.setHeader("Access-Control-Allow-Origin",  "*");
        resp.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp.setHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
        sendResponse(socket, resp);
        return;
    }

    // Попытка найти подходящий маршрут в роутере
    HttpResponse resp;
    if (!m_router.dispatch(req, resp))
    {
        resp.statusCode = 404;
        resp.setJson();
        resp.body = R"({"success":false,"error":{"code":404,"message":"Route not found"}})";
    }

    // Add CORS headers to every response
    resp.setHeader("Access-Control-Allow-Origin",  "*");
    resp.setHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");

    qDebug() << "[HTTP] processRequest:" << req.method << req.path << "->" << resp.statusCode;
    sendResponse(socket, resp);
}

HttpRequest HttpsServer::parseRequest(const QByteArray &rawData)
{
    HttpRequest req;
    int headerEnd = rawData.indexOf("\r\n\r\n");
    QByteArray headerPart =rawData.left(headerEnd);
    req.body = rawData.mid(headerEnd + 4);

    //Разбиение по линиям request line + headers
    QList<QByteArray> lines = headerPart.split('\n');
    if (lines.isEmpty())
        return req;

    //Request part
    QString requestLine = QString::fromLatin1(lines[0].trimmed());
    QStringList parts = requestLine.split(' '); //разбиваем по пробелам ex: POST /login?id=56 HTTP/1.1
    if (parts.size() >= 2)
    {
        req.method = parts[0].toUpper();    // Установка метода (POST GET и тд)
        QString fullPath = parts[1];        // Путь /login?id=56
        int qmark = fullPath.indexOf('?');  //Есть ли данные в url

        if (qmark >= 0) // Если данные в строке есть
        {
            req.path = fullPath.left(qmark); // Только путь POST -> left берет qmark символов с начала
            req.rawQuery = fullPath.mid(qmark + 1);     // Только данные в url: id=56
            QUrlQuery uq(req.rawQuery);

            for (const auto &p : uq.queryItems())
            {
                req.queryParams[p.first] = p.second;
            }
        }
        else // Если данных в строке не найдено
        {
            req.path = fullPath;
        }
    }

    // Установка Headers
    for (int i = 1; i < lines.size(); ++i)
    {
        QString line = QString::fromLatin1(lines[i].trimmed());
        int dataStart = line.indexOf(':'); // Позиция в строке после которой начинается значение заголовка, пример: Host: example.com
        if (dataStart > 0)
        {
            req.headers[line.left(dataStart).trimmed().toLower()] = line.mid(dataStart + 1).trimmed();
        }
    }

    return req;
}

//----- Response writer -------------

void HttpsServer::sendResponse(QTcpSocket *socket, const HttpResponse &resp)
{
    QByteArray raw;
    raw += "HTTP/1.1 " + QString::number(resp.statusCode).toLatin1() + " " + statusText(resp.statusCode).toLatin1() + "\r\n";
    raw += "Date: " + QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy hh:mm:ss").toLatin1() + " GMT\r\n";
    raw += "Server: YaDisk-Backend/1.0\r\n";
    raw += "Content-Length: " + QString::number(resp.body.size()).toLatin1() + "\r\n";
    raw += "Connection: close\r\n";

    for (auto it = resp.headers.constBegin(); it != resp.headers.constEnd(); ++it)
    {
        raw += it.key().toLatin1() + ": " + it.value().toLatin1() + "\r\n";
    }
    raw += "\r\n";
    raw += resp.body;

    socket->write(raw);
    socket->flush();
    socket->disconnectFromHost();
}

QString HttpsServer::statusText(int code)
{
    static const QMap<int,QString> texts =
    {
        {200,"OK"},{201,"Created"},{204,"No Content"},
        {400,"Bad Request"},{401,"Unauthorized"},{403,"Forbidden"},
        {404,"Not Found"},{405,"Method Not Allowed"},{409,"Conflict"},
        {413,"Payload Too Large"},{500,"Internal Server Error"}
    };
    return texts.value(code, "Unknown");
}


//----- TCP connection handling -------------

void HttpsServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    m_buffers[socket] += socket->readAll();
    QByteArray &buf = m_buffers[socket];

    //Ждем пока не придет http запрос полностью
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;

    int contentLength = 0;
    QByteArray headerSection = buf.left(headerEnd);
    static QRegularExpression clRe("Content-Length:\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch clMatch = clRe.match(QString::fromLatin1(headerSection));
    if (clMatch.hasMatch())
        contentLength = clMatch.captured(1).toInt();

    int totalExpected = headerEnd + 4 + contentLength;
    if (buf.size() < totalExpected) return; //body полностью не получено

    QByteArray requestData = buf.left(totalExpected);
    buf = buf.mid(totalExpected);

    processRequest(socket, requestData);
}

void HttpsServer::onNewConnection()
{
    while (m_tcpServer->hasPendingConnections())
    {
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, &HttpsServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &HttpsServer::onDisconnected);
        m_buffers[socket] = QByteArray();
    }
}

void HttpsServer::onDisconnected()
{

}
