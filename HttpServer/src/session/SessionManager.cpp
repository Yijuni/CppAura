#include "SessionManager.h"

#include <iomanip>
#include <iostream>
#include <sstream>
namespace http
{

namespace session
{
// 初始化会话管理器，设置会话存储对象和随机数生成器
http::session::SessionManager::SessionManager(std::unique_ptr<SessionStorage> storage)
    :storage_m(std::move(storage)),
    rng_m(std::random_device{}()) //随机数生成器，用于生成随机的会话id
{
}

// 从请求中获取或创建会话，也就是说，如果请求中包含会话ID，则从存储中加载会话，否则创建一个新的会话
std::shared_ptr<Session> http::session::SessionManager::getSession(const HttpRequest &req, HttpResponse *resp)
{
    std::string sessionId = getSessionIdFromCookie(req);

    std::shared_ptr<Session> session;

    if(!sessionId.empty()){
        session = storage_m->load(sessionId);
    }

    //不存在或者过期
    if(!session || session->isExpired()){
        sessionId = generateSessionId();
        session = std::make_shared<Session>(sessionId,this);
        setSessionCookie(sessionId,resp);
    }else{
        session->setManager(this);
    }

    //刷新过期时间
    session->refresh();
    storage_m->save(session);
    return session;
}

//移除某个session
void http::session::SessionManager::destroySession(const std::string &sessionId)
{
    storage_m->remove(sessionId);

}

//不同的存储方式，清理过期session方式不同
void http::session::SessionManager::cleanExpiredSessions()
{
    storage_m->clearExpired();
}

// 生成唯一的会话标识符，确保会话的唯一性和安全性
std::string http::session::SessionManager::generateSessionId()
{
    std::stringstream ss;
    std::uniform_int_distribution<> dist(0, 15);

    // 生成32个字符的会话ID，每个字符是一个十六进制数字
    for (int i = 0; i < 32; ++i)
    {
        ss << std::hex << dist(rng_m);
    }
    return ss.str();
}

std::string http::session::SessionManager::getSessionIdFromCookie(const HttpRequest &req)
{
    std::string sessionId;
    std::string cookie = req.getHeader("Cookie");

    if(!cookie.empty()){
        size_t pos = cookie.find("sessionId=");
        if(pos!=std::string::npos){
            pos+=10;
            size_t end = cookie.find(';',pos);
            if(end!=std::string::npos){
                sessionId = cookie.substr(pos,end-pos);
            }else{
                sessionId = cookie.substr(pos);
            }
        }
    }
    return sessionId;
}

void http::session::SessionManager::setSessionCookie(const std::string &sessionId, HttpResponse *resp)
{
    //设置会话I达到响应头
    std::string cookie = "sessionId=" + sessionId + "; Path=/; HttpOnly";
    resp->addHeader("Set-Cookie",cookie);
}

} //namespace session
}//namespace http
