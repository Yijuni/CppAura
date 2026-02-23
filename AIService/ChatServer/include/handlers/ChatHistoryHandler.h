#pragma once
#include "RouterHandler.h"
#include "ChatServer.h"

// 获取某个历史会话的所有信息
class ChatHistoryHandler : public http::router::RouterHandler
{
public:
    explicit ChatHistoryHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    ChatServer* server_;
};