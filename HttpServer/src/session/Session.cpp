#include "Session.h"
#include "SessionManager.h"

namespace http
{

namespace session
{
http::session::Session::Session(const std::string &sessionId, SessionManager *sessionManager, int maxAge)
    : sessionId_m(sessionId),
    sessionManager_m(sessionManager),
    maxAge_m(maxAge)
{
    //重设过期时间
    refresh();
}

//检查会话是否过期
bool http::session::Session::isExpired() const
{
    return std::chrono::system_clock::now() >= expiryTime_m;
}

//重设过期时间
void http::session::Session::refresh()
{
    expiryTime_m = std::chrono::system_clock::now() + std::chrono::seconds(maxAge_m);
}

//设置会话数据
void http::session::Session::setValue(const std::string &key, const std::string &value)
{
    data_m[key] = value;
    //设置了manager，则自动保存更改
    if(sessionManager_m){
        sessionManager_m->updateSession(shared_from_this());
    }
}

//获取会话数据
std::string http::session::Session::getValue(const std::string &key) const
{
    auto iter = data_m.find(key);
    if(iter==data_m.end()){
        return "";
    }
    return iter->second;
}

//删除会话数据
void http::session::Session::remove(const std::string &key)
{
    data_m.erase(key);
}

//清空会话数据
void http::session::Session::clear()
{
    data_m.clear();
}

} //namespace session
}//namespace htpp
