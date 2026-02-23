#pragma once
#include "RouterHandler.h"
#include "ChatServer.h"

//处理图像数据,识别图像
class AIUploadSendHandler : public http::router::RouterHandler
{
public:
    explicit AIUploadSendHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};