#include "ChatLogoutHandler.h"

void ChatLogoutHandler::handle(const http::HttpRequest &req, http::HttpResponse *resp)
{
    auto contentType = req.getHeader("Content-Type");
    //验证消息完整性
    if (contentType.empty() || contentType != "application/json" || req.getBody().empty())
    {
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(0);
        resp->setBody("");
        return;
    }

    try
    {
        //根据cookie获取用户会话
        auto session = server_->getSessionManager()->getSession(req, resp);

        int userId = std::stoi(session->getValue("userId"));

        //清空会话状态
        session->clear();

        //从内存移除会话
        server_->getSessionManager()->destroySession(session->getId());

        //解析请求体
        json parsed = json::parse(req.getBody());

        {  
            // 移除会话
            std::lock_guard<std::mutex> lock(server_->mutexForOnlineUsers_);
            server_->onlineUsers_.erase(userId);
        }

        //返回信息
        json response;
        response["message"] = "logout successful";
        std::string responseBody = response.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(responseBody.size());
        resp->setBody(responseBody);
    }
    catch (const std::exception& e)
    {

        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}