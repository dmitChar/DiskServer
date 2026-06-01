#ifndef ROUTER_H
#define ROUTER_H

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMap>

struct StreamingInfo
{
    StreamingInfo() = default;

    StreamingInfo(const QString &path, qint64 offset_, qint64 length_, qint64 fileSize_, bool isRange_, bool useChunk)
        : filePath(path)
        , offset(offset_)
        , length(length_)
        , fileSize(fileSize_)
        , isRange(isRange_)
        , useChunkedTE(useChunk)
    {}

    QString filePath; // Абсолютный путь к файлу на диске
    qint64 offset = 0; // Начальный байт(для range)
    qint64 length = -1; // Количество байт для отправки; -1 = до конца
    qint64 fileSize = 0; // Размер файла (для content-range)
    bool isRange = false;
    bool useChunkedTE = true;
    // Transfer-Encoding: chunked:
    // true - для полных файлов, Content-Length неизвестен заранее
    // false - для Range ответов, Content-Length = length
};

struct HttpRequest
{
    QString method;                     //Метод - POST, GET, PUT ...
    QString path;                       //Путь - /api/login ...
    QString rawQuery;                   //Данные переданные в url(в формате string) - id=5&name=igor
    QMap<QString, QString> queryParams; //Данные переданные в url(в формате map) - QMap{ ["id"] = "5"}, QMap{ ["name"] = "igor"}
    QMap<QString, QString> headers;     // map из названия заголовка и его значения: ["Content-Type"] = "application/json; charset=utf-8"
    QByteArray body;                    // Переданные данные в теле запроса

    QString getHeaderData(const QString &headerName) const
    {
        return headers.value(headerName.toLower());
    }

    QString getQueryData(const QString &queryName) const
    {
        return queryParams.value(queryName);
    }
};


struct HttpResponse
{
    int statusCode = 200;
    QMap<QString, QString> headers;
    QByteArray body;

    // Когда streaming == true -> body игнорируется
    // HttpServer создает ChunkResponeWriter и передает ему socket + streamInfo
    bool streaming = false;
    StreamingInfo streamInfo;

    void setJson()
    {
        headers["Content-Type"] = "application/json; charset=utf-8";
    }
    void setContentType(const QString &ct)
    {
        headers["Content-Type"] = ct;
    }
    void setHeader(const QString &k, const QString &v)
    {
        headers[k] = v;
    }
};

using RouteHandler = std::function<HttpResponse(const HttpRequest &req,
                                                const QMap<QString, QString> &params)>;

// Структура для хранения пути
struct Route
{
    QString            method;
    QRegularExpression pattern;
    QStringList        paramNames;
    RouteHandler       handler;
};



class Router
{
public:
    Router();
    void addRoute(const QString &method, const QString pathPattern, RouteHandler handler);
    bool dispatch(const HttpRequest &req, HttpResponse &resp);
private:
    static QPair<QRegularExpression, QStringList> compilePattern(const QString &pattern);

    QList<Route> m_routes;

};




#endif // ROUTER_H
