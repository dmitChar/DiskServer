#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCommandLineParser>
#include <QDir>

#include <csignal>

#include "httpsserver.h"
#include "database/databasemanager.h"
#include "middleware/authmiddleware.h"
#include "handlers/authhandler.h"
#include "handlers/filehandler.h"
#include "handlers/userhandler.h"

static QCoreApplication *g_app = nullptr;

static void signalHandler(int)
{
    qInfo() << "\n [MAIN] Shutting down...";
    if (g_app)
        g_app->quit();
}

struct AppConfig
{
    QString host = "0.0.0.0";
    quint16 port = 8080;
    QString dbPath ="./yadisk.db";
    QString storageRoot ="./storage";
    quint64 maxFileSizeMb = 5120;
    QString jwtSecret = "biba";
    int tokenExpiryHours = 1;
    qint64 defaultQuotaGb = 10;
};

AppConfig loadConfig(const QString &path)
{
    AppConfig cfg;
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[MAIN] Ошибка чтения файла конфигурации:" << path <<  "\nИспользование дефолтных настроек";
        return cfg;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull())
    {
        qWarning() << "[MAIN] Ошибка конфига JSON";
        return cfg;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("server"))
    {
        QJsonObject s = obj["server"].toObject();
        cfg.host = s.value("host").toString();
        cfg.port = static_cast<quint16>(s.value("port").toInt());
    }

    if (obj.contains("database"))
    {
        cfg.dbPath = obj["database"].toObject().value("path").toString();
    }

    if (obj.contains("storage"))
    {
        QJsonObject s = obj["storage"].toObject();
        cfg.storageRoot = s.value("rootPath").toString();
        cfg.maxFileSizeMb = s.value("maxFileSizeMb").toInt();
    }

    if (obj.contains("auth"))
    {
        QJsonObject s = obj["auth"].toObject();
        cfg.jwtSecret = s.value("jwtSecret").toString();
        cfg.tokenExpiryHours = s.value("tokenExpiryHours").toInt();
    }

    if (obj.contains("quota"))
    {
        cfg.defaultQuotaGb = obj["quota"].toObject().value("defaultUserQuotaGb").toInt();
    }
    qDebug() << "[MAIN] Файл конфигурации был успешно прочитан";

    return cfg;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("Cloud-Disk-Backend");
    g_app = &app;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    //CLI
    QCommandLineParser parser;
    parser.setApplicationDescription("Cloud Disk REST server");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt({"c", "config"}, "Path to config.json", "config", "./config.json");
    parser.addOption(configOpt);
    parser.process(app);

    QDir dir = QDir::currentPath();
    QString configPath = parser.value(configOpt);
    AppConfig cfg = loadConfig(":/configs/config.json");

    qInfo() << "╔══════════════════════════════════╗";
    qInfo() << "║   Cloud Disk Backend  v1.0.0     ║";
    qInfo() << "╚══════════════════════════════════╝";
    qInfo() << "[Config] host        =" << cfg.host;
    qInfo() << "[Config] port        =" << cfg.port;
    qInfo() << "[Config] db          =" << cfg.dbPath;
    qInfo() << "[Config] storage     =" << cfg.storageRoot;
    qInfo() << "[Config] maxFileMb   =" << cfg.maxFileSizeMb;
    qInfo() << "[Config] tokenExpiry =" << cfg.tokenExpiryHours << "h";

    // Убедиться что путь для хранения данных существует
    QDir().mkpath(cfg.storageRoot);

    //DB
    auto *db = new DatabaseManager(&app);
    if (!db->open(cfg.dbPath))
    {
        qCritical() << "[MAIN] Cannot open DataBase. Exiting...";
        return 1;
    }

    //---- Middleware
    auto *auth = new AuthMiddleware(db, cfg.jwtSecret);

    //---- Constants
    const qint64 defaultQuotaBytes = cfg.defaultQuotaGb * 1024LL * 1024LL * 1024LL; // Перевод в Гб
    const qint64 maxFileSizeBytes = cfg.maxFileSizeMb * 1024LL * 1024LL;

    //---- Handlers
    auto *authHandler = new AuthHandler(db, auth, defaultQuotaBytes, cfg.tokenExpiryHours, &app);
    auto *fileHandler = new FileHandler(db, auth, cfg.storageRoot, maxFileSizeBytes, &app);
    auto *userHandler = new UserHandler(db, auth, &app);

    //---- HTTP Server
    auto *server = new HttpsServer(&app);
    server->configure(db, auth, authHandler, fileHandler, userHandler);

    if (!server->start(cfg.host, cfg.port))
    {
        qCritical() << "[MAIN] Failed to start server! Exiting...";
        return 1;
    }




    return app.exec();
}
