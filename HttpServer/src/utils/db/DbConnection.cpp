#include "db/DbConnection.h"
namespace http 
{
namespace db 
{
http::db::DbConnection::DbConnection(const std::string &host, const std::string &user, const std::string &password, const std::string &database)
    :host_m(host),user_m(user),password_m(password),database_m(database)
{
    try{
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        conn_m.reset(driver->connect(host_m,user_m,password_m));

        if(conn_m){
            //选择数据库（可能有多个）
            conn_m->setSchema(database_m);
            
            //启动自动重连,当连接因网络波动断开时，下次操作自动尝试重连。
            //最好删除不然一直警报，重连应该自己控制
            // conn_m->setClientOption("OPT_RECONNECT","false");

            //设置连接超时，避免数据库没响应导致程序卡死
            conn_m->setClientOption("OPT_CONNECT_TIMEOUT","10");
            //禁止多语句执行
            conn_m->setClientOption("multi_statements", "false");

            //设置字符集
            std::unique_ptr<sql::Statement> stmt(conn_m->createStatement());
            stmt->execute("SET NAMES utf8mb4");

            LOG_INFO << "Database connection established";
        }
    }
    catch(const sql::SQLException& e){
        LOG_ERROR << "Failed to create database connection: " << e.what();
        throw DbException(e.what());
    }
}

http::db::DbConnection::~DbConnection()
{
    try{
        cleanup();
    }catch(...){ //捕获所有异常
        //析构函数禁止抛出异常
    }
    LOG_INFO << "Database connection closed";
}

bool http::db::DbConnection::isValid()
{
    try
    {
        if(!conn_m) return false;
        std::unique_lock<std::mutex> lock(mutex_m);
        std::unique_ptr<sql::Statement> stmt(conn_m->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return (rs && rs->next());
    }
    catch(const std::exception& e)
    {
        //如果执行过程出现失败，就会抛出异常，返回false即可
        return false;
    }
}

void http::db::DbConnection::reconnect()
{
    std::unique_lock<std::mutex> lock(mutex_m);
    try{
        if(conn_m){
            conn_m->reconnect();
        }else{
            sql::mysql::MySQL_Driver* driver = sql::mysql::get_driver_instance();
            conn_m.reset(driver->connect(host_m,user_m,password_m));
            conn_m->setSchema(database_m);
        }
    }catch(const sql::SQLException& e){
        LOG_ERROR << "Reconnect failed: " << e.what();
        throw DbException(e.what());
    }
}

void http::db::DbConnection::cleanup()
{
    std::unique_lock<std::mutex> lock(mutex_m);
    try{
        // 确保事务都已经提交，确保所有事务都已完成, 确保不在事务中：防止前一个请求的事务影响下一个
        if(!conn_m->getAutoCommit()){
            conn_m->rollback();
            conn_m->setAutoCommit(true);
        }

    }   
    catch(const std::exception& e){
        LOG_WARN << "Error cleaning up connection: " << e.what();
        try 
        {
            reconnect();
        } 
        catch (...) 
        {
            // 忽略重连错误
        }
    }
}

//监测连接是否有效
bool DbConnection::ping()
{
    try{
        if(!conn_m){
            return false;
        }
        // 使用锁防止与其它操作并发使用同一连接
        std::unique_lock<std::mutex> lock(mutex_m);
        std::unique_ptr<sql::Statement> stmt(conn_m->createStatement());
        std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery("SELECT 1"));
        return (rs && rs->next());
    }catch(const sql::SQLException& e){
        LOG_ERROR<<"Ping failed: "<<e.what();
        return false;
    }
}
}
}