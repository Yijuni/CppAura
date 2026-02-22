#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "router/Router.h"
#include "session/SessionManager.h"
#include "middleware/MiddlewareChain.h"
#include "middleware/cors/CorsMiddleware.h"
#include "ssl/SslConnection.h"
#include "ssl/SslContext.h"

class HttpResponse;
class HttpRequest;

namespace http
{
class HttpServer : muduo::noncopyable{
public:
    using HttpCallback = std::function<void(const http::HttpRequest&,http::HttpResponse*)>;

    //构造函数,默认只有一个 socket 可以绑定到该端口（常规 bind/accept）
    HttpServer(int port,const std::string& name,
        bool useSSL = false,
        muduo::net::TcpServer::Option option= muduo::net::TcpServer::kNoReusePort);
    
    //设置loop线程数目
    void setThreadNum(int numThread){
        server_m.setThreadNum(numThread);
    }

    //开始服务
    void start();

    //获取IO loop的指针
    muduo::net::EventLoop* getLoop() const{
        return server_m.getLoop();
    }

    //设置HTTP回调函数
    void setHttpCallBack(const HttpCallback& cb){
        httpCallBack_m = cb;
    }
    /** 
     *  简单请求 注册函数回调
     *  复杂请求 注册对象回调
    */
       
    //注册Get静态路由处理器（回调函数）
    void Get(const std::string& path,const HttpCallback& cb){
        router_m.registerCallback(HttpRequest::kGet,path,cb);
    }
    //注册Get静态路由处理器（对象回调)
    void Get(const std::string& path,router::Router::HandlerPtr handler){
        router_m.registerHandler(HttpRequest::kGet,path,handler);
    }
    //注册Post静态路由处理器（回调函数）
    void Post(const std::string& path, const HttpCallback& cb)
    {
        router_m.registerCallback(HttpRequest::kPost, path, cb);
    }
    //注册Post静态路由处理器（对象回调)
    void Post(const std::string& path, router::Router::HandlerPtr handler)
    {
        router_m.registerHandler(HttpRequest::kPost, path, handler);
    }

    //注册动态路由处理器（对象回调）
    void addRoute(HttpRequest::Method method, const std::string& path, router::Router::HandlerPtr handler)
    {
        router_m.addRegexHandler(method, path, handler);
    }
    //注册动态路由处理器（函数回调）
    void addRoute(HttpRequest::Method method, const std::string& path, const router::Router::HandlerCallback& callback)
    {
        router_m.addRegexCallback(method, path, callback);
    }

    //设置Seesion会话管理器
    void setSessionManager(std::unique_ptr<session::SessionManager> manager){
        sessionManager_m = std::move(manager);
    }
    //获取Session会话管理器
    session::SessionManager* getSessionManager() const
    {
        return sessionManager_m.get();
    }   

    //添加中间件方法
    void addMiddleware(std::shared_ptr<middleware::Middleware> middleware){
        middlewareChain_m.addMiddleware(middleware);
    }

    //设置ssl使用
    void enableSSL(bool enable){
        useSSL_m = enable;
    }

    //设置ssl连接配置
    void setSslConfig(const ssl::SslConfig& config);

private:
    //初始化
    void initialize();

    //设置连接回调,初次accept并创建tcpconnction的时候就会被自动调用
    void onConnection(const muduo::net::TcpConnectionPtr& conn);

    //设置消息回调，有客户端发来的消息时调用
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                    muduo::net::Buffer* buf,muduo::Timestamp receiveTime);
    
    //这个函数有onMessage调用，处理请求,并发送响应到客户端                
    void onRequest(const muduo::net::TcpConnectionPtr&, const HttpRequest&);

    //这个函数会被onRequest调用,处理请求结果放进response
    void handleRequest(const HttpRequest& req, HttpResponse* resp);
    
    muduo::net::InetAddress listenAddr_m;//监听地址 hhtp:80 htpps:443
    muduo::net::TcpServer server_m;//接受连接的服务器本体
    muduo::net::EventLoop mainLoop_m;//主循环
    HttpCallback httpCallBack_m;
    router::Router router_m;//路由
    std::unique_ptr<session::SessionManager> sessionManager_m;//会话管理器（基于内存的目前）
    middleware::MiddlewareChain middlewareChain_m;//中间件链（目前只实现了跨域访问)
    std::unique_ptr<ssl::SslContext> sslCtx_m;//sll上下文,多个ssl连接共用一个上下文（配置）
    bool useSSL_m;//使用ssl？
    //TcpConnectionPtr到SslConnectionPtr的映射，
    // 如果启用了ssl，ssl负责数据的加密解密，会调用TcpConnectionPtr的发送函数发送数据
    //TcpConnecPtr会把接收消息回调函数变成ssl的,相当于再TcpConnect上面又套了一层sslConnection
    std::unordered_map<muduo::net::TcpConnectionPtr,std::shared_ptr<ssl::SslConnection>> sslConns_m;
};

}