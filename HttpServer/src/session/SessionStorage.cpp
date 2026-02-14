#include "SessionStorage.h"

namespace http
{

namespace session
{
void http::session::MemorySessionStorage::save(std::shared_ptr<Session> session)
{
    //创建副本并保存
    sessions_m[session->getId()] = session;
}

std::shared_ptr<Session> http::session::MemorySessionStorage::load(const std::string &sessionId)
{
    auto iter = sessions_m.find(sessionId);
    //不存在
    if(iter==sessions_m.end()){
        return nullptr;
    }
    //过期删除
    if(iter->second->isExpired()){
        sessions_m.erase(iter);
        return nullptr;
    }
    return iter->second;
}

void http::session::MemorySessionStorage::remove(const std::string &sessionId)
{
    auto iter = sessions_m.find(sessionId);
    if(iter==sessions_m.end()){
        return;
    }
    sessions_m.erase(iter);
}
void MemorySessionStorage::clearExpired()
{
    for(auto &pair : sessions_m){
        if(pair.second->isExpired()){
            sessions_m.erase(pair.first);
        }
    }
}

}
}