#pragma once
#include "RouterHandler.h"
#include "ChatServer.h"

// 加载图像识别页面
class AIUploadHandler : public http::router::RouterHandler
{
public:
    explicit AIUploadHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;

private:
    ChatServer* server_;
};