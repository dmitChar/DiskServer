#ifndef CHUNKSTREAMINGSTRATEGY_H
#define CHUNKSTREAMINGSTRATEGY_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QMap>
#include "StreamingStrategy.h"

class ChunkStreamingStrategy : public StreamingStrategy
{
    Q_OBJECT
public:
    explicit ChunkStreamingStrategy(QObject *parent);
    ~ChunkStreamingStrategy() override;

protected:
    void sendHeaders() override;
    void sendNextChunk() override;
};

#endif // CHUNKSTREAMINGSTRATEGY_H
