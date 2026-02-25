#include "http/HttpServer.h"
namespace http
{

    HttpServer::HttpServer(int port, const std::string &name, bool useSSL, muduo::net::TcpServer::Option option)
        : listenAddr_m(port), server_m(&mainLoop_m, listenAddr_m, name, option),
          httpCallBack_m(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
    {
        // 初始化
        initialize();
    }

    // a启动服务器
    void HttpServer::start()
    {
        LOG_WARN << "HttpServer[" << server_m.name() << "] starts listening on" << server_m.ipPort();

        // 开始监听连接（监听端口，往epoll注册fd的可读事件）
        server_m.start();
        // 开始事件监听（可读事件发生，例如端口来了新的连接，就会出发accept）
        mainLoop_m.loop();
    }

    // 设置SSL的配置，config内部包含各种文件的路径
    void http::HttpServer::setSslConfig(const ssl::SslConfig &config)
    {
        if (useSSL_m)
        {
            sslCtx_m = std::make_unique<ssl::SslContext>(config);
            if (!sslCtx_m->initialize())
            {
                LOG_ERROR << "Failed to initialize SSL context";
                abort();
            }
        }
    }

    void HttpServer::initialize()
    {
        // 设置连接建立回调函数
        server_m.setConnectionCallback(std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
        // 设置消息回调
        server_m.setMessageCallback(std::bind(&HttpServer::onMessage, this,
                                              std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }
    void HttpServer::onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        { // 刚建立连接
            if (useSSL_m)
            {
                // 这一步，TcpConnectionPtr的onMessage回调被修改成了ssl内部的onRead
                auto sslConn = std::make_unique<ssl::SslConnection>(conn, sslCtx_m.get());
                // 这里必须注册回调，给ssl，因为onRead解密后会调用回调来真正的处理消息
                sslConn->setMessageCallback(std::bind(&HttpServer::onMessage, this,
                                                      std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
                // 建立sslcontection和tcpconnection的映射
                sslConns_m[conn] = std::move(sslConn);
                // 调用一次握手
                sslConns_m[conn]->startHandshake();
            }
            // 设置这个连接的HttpContext上下文，用来专门解析该连接的Http请求
            //  setContext传入后被any接受
            //  boost::any 的行为：它会在内部动态分配内存，拷贝构造你传入的对象。
            conn->setContext(HttpContext());
        }
        else
        { // 结束连接
            if (useSSL_m)
            {
                sslConns_m.erase(conn);
            }
        }
    }
    void HttpServer::onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp receiveTime)
    {
        try
        {
            // 这时候接收到的消息，必然是已经解密的，读出这个连接的HttpConext来解析Http请求

            // 获取TcpConnection内部保存的HttpConext对象(每次修改都是改的同一个)
            HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());
            if (!context->parseRequest(buf, receiveTime))
            {
                // 解析出错
                conn->send("HTTP/1.1 400 Bad request\r\n\r\n"); // 返回也是严格按照http请求数据
                conn->shutdown();
            }
            // 缓冲区解析出了一个完整的http报文
            if (context->gotAll())
            {
                // 调用处理函数
                onRequest(conn, context->request());
                // 重置内部状态，开启新一轮的解析
                context->reset();
            }
        }
        catch (const std::exception &e)
        {
            // 捕获异常，返回错误信息
            LOG_ERROR << "Exception in onMessage: " << e.what();
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }
    }
    void HttpServer::onRequest(const muduo::net::TcpConnectionPtr &conn, const HttpRequest &request)
    {
        // 判断要不要持久连接
        const std::string connection = request.getHeader("Connection");
        bool close = ((connection=="close") || (request.getVersion()=="HTTP/1.0" && connection!="Keep-Alive"));
        HttpResponse response(close);

        //执行真正处理请求逻辑的函数
        httpCallBack_m(request,&response);

        // 可以给response设置一个成员，判断是否请求的是文件，如果是文件设置为true，并且存在文件位置在这里send出去。
        muduo::net::Buffer buf;
        response.appendToBuffer(&buf);
        // 打印完整的响应内容用于调试
        LOG_INFO << "Sending response:\n" << buf.toStringPiece().as_string();
        if (useSSL_m) {
            auto it = sslConns_m.find(conn);
            if (it != sslConns_m.end() && it->second) {
                it->second->send(buf.peek(), buf.readableBytes());
            } else {
                // 如果 SSL 映射丢失，回退到直接发送以避免丢包
                conn->send(&buf);
            }
        } else {
            conn->send(&buf);
        }
        
        // 如果是短连接的话，返回响应报文后就断开连接
        if (response.closeConnection())
        {
            conn->shutdown();
        }
    }

    // 执行请求对应的路由处理函数
    void HttpServer::handleRequest(const HttpRequest &req, HttpResponse *resp)
    {
        try{
            //处理请求前的中间件
            HttpRequest mutableReq = req;
            middlewareChain_m.processBefore(mutableReq);

            if(!router_m.route(mutableReq,resp)){
                LOG_INFO << "请求的啥，url：" << req.method() << " " << req.path();
                LOG_INFO << "未找到路由，返回404";
                resp->setStatusCode(HttpResponse::k404NotFound);
                resp->setStatusMessage("Not Found");
                resp->setCloseConnection(true);
            }
        }catch(const HttpResponse& res){
            // 处理中间件抛出的响应（如CORS预检请求），这样后续的中间件以及处理逻辑都不会执行了
            *resp = res;
        }
        catch(std::exception &e){
            // 错误处理
            resp->setStatusCode(HttpResponse::k500InternalServerError);
            resp->setBody(e.what());
        }
    }
}