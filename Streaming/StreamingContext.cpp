#include "StreamingContext.h"

StreamingContext::StreamingContext()
{

}

void StreamingContext::setStrategy(std::unique_ptr<StreamingStrategy> strategy)
{
    m_strategy = std::move(strategy);
}

void StreamingContext::execute(QTcpSocket *socket, const HttpResponse &resp)
{
    if (!m_strategy->start(socket, resp))
    {
        qDebug() << "[HTTP Server] Streaming start failed for" << resp.streamInfo.filePath;
    }
}
