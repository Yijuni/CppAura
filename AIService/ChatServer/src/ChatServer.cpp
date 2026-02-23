#include "ChatServer.h"
#include "ChatLoginHandler.h"
#include "ChatRegisterHandler.h"
#include "ChatLogoutHandler.h"
#include "ChatHandler.h"
#include "ChatEntryHandler.h"
#include "ChatSendHandler.h"
#include "AIMenuHandler.h"
#include "AIUploadSendHandler.h"
#include "AIUploadHandler.h"
#include "ChatHistoryHandler.h"

#include "ChatCreateAndSendHandler.h"
#include "ChatSessionsHandler.h"
#include "ChatSpeechHandler.h"

#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServer.h"

ChatServer::ChatServer(int port, const std::string &name,
                       muduo::net::TcpServer::Option option)
    : httpServer_m(port, name, false, option)
{
    initialize();
}

void ChatServer::setThreadNum(int numThread)
{
    // 设置muduo loop的线程数
    httpServer_m.setThreadNum(numThread);
}

// 服务启动
void ChatServer::start()
{
    httpServer_m.start();
}

void ChatServer::initChatMessage()
{
    LOG_WARN << "initChatMessage start !! ";
    readDataFromMySQL();
    LOG_WARN << "initChatMessage success !!";
}

void ChatServer::initialize()
{
    LOG_WARN << "ChatServer initialize start  ! ";
    
    //todo:修改掉硬编码
    http::MysqlUtil::init("tcp://127.0.0.1:3306", "root", "123456", "ChatHttpServer", 5);

    LOG_WARN << "初始化session组件! ";
    initializeSession();

    LOG_WARN << "初始化http服务中间件组件! ";
    initializeMiddleware();

    LOG_WARN << "初始化http服务路由组件! ";
    initializeRouter();
}

void ChatServer::initializeSession()
{
    // 设置Session存储方式
    auto sessionStorage = std::make_unique<http::session::MemorySessionStorage>();

    // 设置session管理器
    auto sessionManager = std::make_unique<http::session::SessionManager>(std::move(sessionStorage));

    // 给http服务器设置session管理器
    setSessionManager(std::move(sessionManager));
}

// 给http服务器的路由器router注册路径到处理器的映射，等新的请求到达会根据路径调用对应处理器
void ChatServer::initializeRouter()
{
    // 默认页面就是登录注册页面
    httpServer_m.Get("/", std::make_shared<ChatEntryHandler>(this));

    httpServer_m.Get("/entry", std::make_shared<ChatEntryHandler>(this));

    httpServer_m.Post("/login", std::make_shared<ChatLoginHandler>(this));

    httpServer_m.Post("/register", std::make_shared<ChatRegisterHandler>(this));

    httpServer_m.Post("/user/logout", std::make_shared<ChatLogoutHandler>(this));

    httpServer_m.Get("/chat", std::make_shared<ChatHandler>(this));

    httpServer_m.Post("/chat/send", std::make_shared<ChatSendHandler>(this));

    httpServer_m.Get("/menu", std::make_shared<AIMenuHandler>(this));

    httpServer_m.Get("/upload", std::make_shared<AIUploadHandler>(this));

    httpServer_m.Post("/upload/send", std::make_shared<AIUploadSendHandler>(this));

    httpServer_m.Post("/chat/history", std::make_shared<ChatHistoryHandler>(this));

    httpServer_m.Post("/chat/send-new-session", std::make_shared<ChatCreateAndSendHandler>(this));

    httpServer_m.Get("/chat/sessions", std::make_shared<ChatSessionsHandler>(this));

    httpServer_m.Post("/chat/tts", std::make_shared<ChatSpeechHandler>(this));
}

void ChatServer::initializeMiddleware()
{
    // 添加预检中间件
    httpServer_m.addMiddleware(std::make_shared<http::middleware::CorsMiddleware>());
}

void ChatServer::readDataFromMySQL()
{
    std::string sql = "SELECT id,username,session_id,is_user,content,ts FROM chat_message ORDER BY ts ASC,id ASC";

    sql::ResultSet *res;

    try
    {
        res = mysqlUtil_m.executeQuery(sql);
    }
    catch (const std::exception &e)
    {
        std::cerr << "MySQL query failed: " << e.what() << std::endl;
        LOG_ERROR << "MySQL query failed: " << e.what();
        return;
    }

    while(res->next()){
        long long user_id = 0;
        std::string session_id; //会话id
        std::string username,content;//用户姓名和对话内容
        long long ts = 0;//会话消息时间戳
        int is_user = 1; //是用户还是ai

        try{
            user_id = res->getInt64("id");
            session_id = res->getString("session_id");
            username = res->getString("username");
            content = res->getString("content");
            ts = res->getInt64("ts");
            is_user = res->getInt("is_user");
        }catch (const std::exception& e) {
            std::cerr << "Failed to read row: " << e.what() << std::endl;
            LOG_ERROR<< "Failed to read row: " << e.what();
            continue; 
        }
        
        //获取用户所有的会话
        auto& userSession = chatInformation[user_id];

        std::shared_ptr<AIHelper> helper;
        //查找该会话是否加入到内存
        auto itSession = userSession.find(session_id);
        //第一次加载到内存
        if(itSession==userSession.end()){
            helper = std::make_shared<AIHelper>();
            userSession[session_id] = helper;
            sessionsIdsMap[user_id].push_back(session_id);
        }else{
            helper = itSession->second;
        }

        //将数据放入该session的内存中，到时候请求会使用所有的对话上下文
        helper->restorMessage(content,ts);
    }
    std::cout << "readDataFromMySQL finished" << std::endl;
}

//暂时用不到
void ChatServer::packageResp(const std::string &version, http::HttpResponse::HttpStatusCode statusCode, const std::string &statusMsg, bool close, const std::string &contentType, int contentLen, const std::string &body, http::HttpResponse *resp)
{

}
