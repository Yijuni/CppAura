#pragma once
#include <queue>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>
#include "DbConnection.h"

namespace http
{
namespace db
{

class DbConnectionPool{
public:
    //单例模式
    static DbConnectionPool& getInstance(){
        static DbConnectionPool instance;
        return instance;
    }

    //初始化连接池
    void init(const std::string& host,const std::string& user,
                const std::string& password,const std::string& darabase,
                size_t poolSize = 10);

    //获取连接
    std::shared_ptr<DbConnection> getConnection();

private:
    //构造函数
    DbConnectionPool();
    //析构函数
    ~DbConnectionPool();

    //禁止拷贝
    DbConnectionPool(const DbConnectionPool&)=delete;
    DbConnectionPool& operator=(const DbConnectionPool&)=delete;

    std::shared_ptr<DbConnection> createConnection();

    //连接状态检查函数，如果有链接断开，需要及时重连
    void checkConnections();

private:
    std::string host_m;
    std::string user_m;
    std::string password_m;
    std::string database_m;
    std::queue<std::shared_ptr<DbConnection>> connections_m;
    std::mutex mutex_m;
    std::condition_variable cond_m;
    //是否初始化
    bool initialized_m = false;
    //检查连接状态的线程
    std::thread checkThread_m;
    std::atomic<bool> stopFlag_m{false};
};

}
}