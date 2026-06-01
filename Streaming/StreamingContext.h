#ifndef STREAMINGCONTEXT_H
#define STREAMINGCONTEXT_H

#include <memory>
#include "StreamingStrategy.h"

class StreamingContext
{
public:
    StreamingContext();
    void setStrategy(std::unique_ptr<StreamingStrategy> strategy);
    void execute(QTcpSocket *socket, const HttpResponse &resp);

private:
    std::unique_ptr<StreamingStrategy> m_strategy;
};

#endif // STREAMINGCONTEXT_H
