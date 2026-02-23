#pragma once
#include "RouterHandler.h"
#include "ChatServer.h"
#include "JsonUtil.h"

class ChatLogoutHandler : public http::router::RouterHandler
{
public:
    explicit ChatLogoutHandler(ChatServer* server) : server_(server) {}
    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:
    ChatServer* server_;
};