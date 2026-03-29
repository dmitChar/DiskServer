#ifndef HTTPSSERVER_H
#define HTTPSSERVER_H

#include <QObject>

class HttpsServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpsServer(QObject *parent = nullptr);
public:
    void start();
    void stop();
    void configure();

private:


signals:
};

#endif // HTTPSSERVER_H
