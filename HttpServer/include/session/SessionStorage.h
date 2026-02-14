#pragma once
#include "Session.h"
#include <memory>

namespace http
{
namespace session
{

//Session存储的抽象接口,可
class SessionStorage
{
public:
    virtual ~SessionStorage() = default;
    virtual void save(std::shared_ptr<Session> session) = 0;
    virtual std::shared_ptr<Session> load(const std::string& sessionId) = 0;
    virtual void remove(const std::string& sessionId) = 0;
    virtual void clearExpired() = 0;
};

// 基于内存的会话存储实现
class MemorySessionStorage : public SessionStorage
{
public:
    void save(std::shared_ptr<Session> session) override;
    std::shared_ptr<Session> load(const std::string& sessionId) override;
    void remove(const std::string& sessionId) override;
    void clearExpired() override;
private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_m;
};

} // namespace session
} // namespace http