#include "db/DbConnectionPool.h"

namespace http
{
namespace db
{


void http::db::DbConnectionPool::init(const std::string &host, const std::string &user, const std::string &password, const std::string &darabase, size_t poolSize)
{
    //加锁，防止多次初始化
    std::unique_lock<std::mutex> lock(mutex_m);
    //只能初始化一次
    if(initialized_m){
        return;
    }

    host_m = host;
    user_m = user;
    password_m = password;
    database_m = darabase;

    //创建连接
    for(size_t i = 0;i<poolSize;i++){
        connections_m.push(createConnection());
    }
    initialized_m = true;
    LOG_INFO << "Database connection pool initialized with " << poolSize << " connections";
}

std::shared_ptr<DbConnection> http::db::DbConnectionPool::getConnection()
{
    std::shared_ptr<DbConnection> conn;
    {
        std::unique_lock<std::mutex> lock(mutex_m);

        while(!stopFlag_m && connections_m.empty()){ //防止假唤醒
            //因为没有初始化才为空
            if(!initialized_m){
                throw DbException("Connection pool not initialized");
            }
            LOG_INFO<<"Waiting for available connection ...";
            cond_m.wait(lock);
        }
        if(stopFlag_m){
            return nullptr;
        }
        conn = connections_m.front();
        connections_m.pop();
    }

    //锁外检查连接是否正常
    try
    {
        //锁外检查连接是否正常
        if(!conn->ping()){
            LOG_WARN << "Connection lost, attempting to reconnect...";
            conn->reconnect();
        }

        // 当用户用完连接（shared_ptr 被销毁）时，自动把连接放回池中，而不是 delete 它！
        // 这是 std::shared_ptr 的自定义删除器（Custom Deleter）用法！
        //返回的智能指针引用计数是1，因为引用计数控制块不是用的同一个
        return std::shared_ptr<DbConnection>(conn.get(),
        [this,conn](DbConnection*){
            try{
                // 在归还连接池前尽量清理连接状态
                conn->cleanup();
            }catch(...){
                // 忽略清理过程中可能的错误，后续重连逻辑会处理不可恢复的连接
            }
            std::unique_lock<std::mutex> lock(this->mutex_m);
            //放回连接池
            connections_m.push(std::move(conn));
            cond_m.notify_one();//如果有等待连接的的唤醒等待的线程
        });
    }catch(const std::exception& e)
    {
        LOG_ERROR << "Failed to get connection: " << e.what();
        {
            //出错放回连接池
            std::lock_guard<std::mutex> lock(mutex_m);
            connections_m.push(conn);
            cond_m.notify_one();
        }
        throw;
    }
}


http::db::DbConnectionPool::DbConnectionPool()
{
    checkThread_m = std::thread(std::bind(&DbConnectionPool::checkConnections,this));
}

http::db::DbConnectionPool::~DbConnectionPool()
{
    std::unique_lock<std::mutex> lock(mutex_m);
    stopFlag_m = true;
    cond_m.notify_all();
    if(checkThread_m.joinable()){
        checkThread_m.join();
    }
    while(!connections_m.empty()){
        connections_m.pop();
    }
    LOG_INFO << "Database connection pool destroyed";
}

std::shared_ptr<DbConnection> http::db::DbConnectionPool::createConnection()
{

    return std::make_shared<DbConnection>(host_m,user_m,password_m,database_m);
}

void http::db::DbConnectionPool::checkConnections()
{
    while(!stopFlag_m){
        try{
            std::queue<std::shared_ptr<DbConnection>> temp;
            {
                std::unique_lock<std::mutex> lock(mutex_m);
                if(connections_m.empty()){
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
                temp = connections_m;
            }

            //锁外检查
            while(!temp.empty()){
                auto front = temp.front();
                temp.pop();
                if(!front->ping()){
                    try{
                        front->reconnect();
                    }catch(const std::exception& e){
                        LOG_ERROR << "Failed to reconnect: " << e.what();
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(60));
        }catch(const std::exception& e){
            LOG_ERROR << "Error in check thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

}
}
