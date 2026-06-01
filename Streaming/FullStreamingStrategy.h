#ifndef FULLSTREAMINGSTRATEGY_H
#define FULLSTREAMINGSTRATEGY_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QMap>
#include "StreamingStrategy.h"

class FullStreamingStrategy : public StreamingStrategy
{
    Q_OBJECT
public:
    explicit FullStreamingStrategy(QObject *parent);

    using StreamingStrategy::StreamingStrategy;
    ~FullStreamingStrategy() override;

protected:
    void sendHeaders() override;
    void sendNextChunk() override;

};

#endif // FULLSTREAMINGSTRATEGY_H
