#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <mysql_driver.h>
#include <mysql/mysql.h>
#include <muduo/base/Logging.h>
#include "DbException.h"

namespace http 
{
namespace db 
{

class DbConnection 
{
public:
    DbConnection(const std::string& host, 
                const std::string& user,
                const std::string& password,
                const std::string& database);
    ~DbConnection();

    // 禁止拷贝
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    bool isValid();
    void reconnect();
    void cleanup();

    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try 
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_m->prepareStatement(sql)
            );
            //递归绑定参数
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            //执行查询结果
            return stmt->executeQuery();
        } 
        catch (const sql::SQLException& e) 
        {
            LOG_ERROR << "Query failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }
    
    template<typename... Args>
    int executeUpdate(const std::string& sql, Args&&... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        try 
        {
            // 直接创建新的预处理语句，不使用缓存
            std::unique_ptr<sql::PreparedStatement> stmt(
                conn_m->prepareStatement(sql)
            );
            //递归绑定参数
            bindParams(stmt.get(), 1, std::forward<Args>(args)...);
            //返回查询结果
            return stmt->executeUpdate();
        } 
        catch (const sql::SQLException& e) 
        {
            LOG_ERROR << "Update failed: " << e.what() << ", SQL: " << sql;
            throw DbException(e.what());
        }
    }

    bool ping();  // 添加检测连接是否有效的方法
private:
     // 辅助函数：递归终止条件,也就是参数包没东西了
    void bindParams(sql::PreparedStatement*, int) {}
    
    //参数展开
    template<typename T,typename... Args>
    void bindParams(sql::PreparedStatement* stmt,int index,T&& value,Args&&... args){
        stmt->setString(index,std::to_string(std::forward<T>(value)));
        //继续展开
        bindParams(stmt,index+1,std::forward<Args>(args)...);
    }

    //对string类型特化,当args第一个参数为string的时候优先使用这个函数
    template<typename... Args>
    void bindParams(sql::PreparedStatement* stmt,int index,const std::string& value,Args&&... args){
        stmt->setString(index,value);
        //继续展开
        bindParams(stmt,index+1,std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<sql::Connection> conn_m;
    std::string                      host_m;
    std::string                      user_m;
    std::string                      password_m;
    std::string                      database_m;
    std::mutex                       mutex_m;
};

} // namespace db
} // namespace http