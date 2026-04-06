#ifndef ROUTER_H
#define ROUTER_H

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QMap>

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
    void addRoute(const QString &method, const QString &pathPattern, RouteHandler handler);
    bool dispatch(const HttpRequest &req, HttpResponse &resp);
private:
    static QPair<QRegularExpression, QStringList> compilePattern(const QString &pattern);

    QList<Route> m_routes;

};

#endif // ROUTER_H
