#pragma once
#include "RouterHandler.h"
#include "MysqlUtil.h"
#include "ChatServer.h"

// 处理网页发来的消息
class ChatSendHandler : public http::router::RouterHandler
{
public:
    explicit ChatSendHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};
