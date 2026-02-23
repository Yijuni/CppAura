#pragma once
#include "RouterHandler.h"
#include "MysqlUtil.h"
#include "ChatServer.h"

//获取所有sessions,AI页面初始化会自动调用获取所有session
class ChatSessionsHandler : public http::router::RouterHandler
{
public:
    explicit ChatSessionsHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    ChatServer* server_;
    http::MysqlUtil     mysqlUtil_;
};