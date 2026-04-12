#include "router.h"

Router::Router()
{

}

void Router::addRoute(const QString &method, const QString pathPattern, RouteHandler handler)
{
    QPair<QRegularExpression, QStringList> result = compilePattern(pathPattern);
    m_routes.append({method.toUpper(), result.first, result.second, std::move(handler)});

}

bool Router::dispatch(const HttpRequest &req, HttpResponse &resp)
{
    // Проходим по всем записанным маршрутам и пытаемся найти подходящий
    for (const Route &route : m_routes)
    {
        // Метод должен совпадать
        if (route.method != req.method && route.method != "*")
            continue;

        //Path должен совпадать с regex
        QRegularExpressionMatch match = route.pattern.match(req.path);
        if (!match.hasMatch())
            continue;

        // Если обе проверки пройдены -> извлекаем :params
        QMap<QString, QString> params;
        for (int i = 0; i < route.paramNames.size(); ++i)
        {
            params[route.paramNames[i]] = match.captured(i+1);
        }

        // Получаем Response для запроса
        resp = route.handler(req, params);
        return true;
    }
    return false;
}

QPair<QRegularExpression, QStringList> Router::compilePattern(const QString &pattern)
{
    QStringList names;

    //Находим все :param в паттерне
    QRegularExpression paramRe(":([a-zA-Z_][a-zA-Z0-9_]*)");
    QRegularExpressionMatchIterator it = paramRe.globalMatch(pattern);

    //Запоминаем имена параметров
    while (it.hasNext())
    {
        names << it.next().captured(1); //token, id ...
    }
    //Заменяем :param -> ([^/]+)
    QString regexStr = pattern;
    regexStr.replace(paramRe, "([^/]+)");

    //Оборачиваем в якоря ^ $
    regexStr = "^" +regexStr + "$";

    return {QRegularExpression(regexStr), names};
}
