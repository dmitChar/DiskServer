#ifndef RANGESTREAMINGSTRATEGY_H
#define RANGESTREAMINGSTRATEGY_H

#include <QObject>
#include "StreamingStrategy.h"

class RangeStreamingStrategy : public StreamingStrategy
{
    Q_OBJECT
public:
    explicit RangeStreamingStrategy(QObject *parent = nullptr);
    ~RangeStreamingStrategy() override;

protected:
    void sendHeaders() override;
    void sendNextChunk() override;
};

#endif // RANGESTREAMINGSTRATEGY_H
