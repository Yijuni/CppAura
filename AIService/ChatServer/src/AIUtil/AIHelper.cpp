#include "AIHelper.h"
#include "MQManager.h"
// 构造函数
AIHelper::AIHelper()
{
    //默认使用阿里云普通对话的大模型
    strategy = StrategyFactory::instance().create("1");
}

void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat)
{
    strategy = strat;
}

void AIHelper::addMessage(int userId, const std::string &userName, bool is_user, const std::string &userInput, std::string sessionId)
{
    LOG_WARN<<"添加新的消息到数据库 userid"<<userId<<" username: "<<userName<<" userinput:"<<userInput;
    auto now = std::chrono::system_clock::now();
    // time_since_epoch() 返回从 UNIX 纪元（1970-01-01 00:00:00 UTC） 到当前时间点的持续时间（duration）
    auto duration = now.time_since_epoch();
    // 将高精度 duration（如纳秒）向下转换为毫秒级精度。
    // 提取底层整数值（如 123），类型通常是 long long 或 int64_t
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    // 将消息与时间戳存入本地缓存(内存),用户和模型的历史对话记录，每次发送请求都要把历史对话发过去
    messages.push_back({userInput,ms});
    //消息队列异步入库(持久化)
    pushMessageToMysql(userId,userName,is_user,userInput,ms,sessionId);
}
void AIHelper::restorMessage(const std::string &userInput, long long ms)
{
    messages.push_back({userInput,ms});
}
/**
 * 整体流程
 * 判断是否需要MCP ：不需要直接构建请求发送给模型获取结果
 * 需要MCP：
 *      1. 构建工具调用请求消息，同时包含之前对话的上下文（模板Prompt）给大模型
 *      2. 大模型按照一定格式返回工具及其参数
 *      3. 调用工具，获取调用工具的结果
 *      4. 根据结果再次构造请求消息，同时包含之前对话上下文，发送给大模型
 *      5. 获取最终结果，返回给客户端
 */
// 可以动态选择请求的模型类型
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId, std::string userQuestion, std::string modelType)
{
    //设置模型策略.每次会话可以选择不同的模型来提问
    setStrategy(StrategyFactory::instance().create(modelType));
    std::cout<<"模型类别："<<modelType<<std::endl;
    //不支持MCP
    if(!strategy->isMCPModel){
        LOG_DEBUG<<"不使用MCP工具";
        //添加
        addMessage(userId,userName,true,userQuestion,sessionId);
        //构建请求消息
        json payload = strategy->buildRequest(this->messages);

        //执行请求
        json response = executeCurl(payload);
        //解析返回消息
        
        std::string answer = strategy->parseResponse(response);

        LOG_DEBUG<<"模型回答:"<<answer;
        //添加消息都按本地存储
        addMessage(userId,userName,false,answer,sessionId);

        if(answer.empty()){
            LOG_ERROR<<"无法解析模型响应";
            return "[Error] 无法解析模型响应";
        }else{
            return answer;
        }
    }

    LOG_DEBUG<<"使用MCP工具";
    //支持MCP,先尝试使用工具调用
    AIConfig config;
    //todo:硬编码需要改掉
    config.loadFromFile("../AIService/ChatServer/resource/tools/config.json");
    std::string tempUserQuestion = config.buildPrompt(userQuestion);
    LOG_DEBUG << "工具调用第一轮提示词 " << tempUserQuestion;

    //这里没有放入数据库，这是放入内存中
    messages.push_back({tempUserQuestion,0});

    //构建请求格式，过往的对话也放上了（这次工具调用已经放进去了）
    json firstReq = strategy->buildRequest(this->messages);
    // 发送消息
    json firstResp = executeCurl(firstReq);
    //解析结果
    std::string aiResult = strategy->parseResponse(firstResp);

    //用完移除这次工具调用的会话提示词
    // 因为这次发送给模型是服务器自身发送的，不属于用户消息
    messages.pop_back();

    LOG_DEBUG << "工具调用AI回复json: " << aiResult;
    //解析这次工具调用的响应（是否需要工具调用)
    AIToolCall call = config.parseAIResponse(aiResult);

    //不调用工具
    if(!call.isToolCall){
        //这此用户提问放进去
        addMessage(userId,userName,true,userQuestion,sessionId);
        //模型回复放进去
        addMessage(userId,userName,false,aiResult,sessionId);

        LOG_DEBUG<<"不使用MCP工具";
        return aiResult;
    }

    //这次使用的工具调用，需要再次发送调用结果给模型
    json toolJson;
    AIToolRegistry registry;

    try{
        // 调用工具获取结果
        toolJson = registry.invoke(call.name,call.args);
        std::cout<<"Tool call success"<<std::endl;
    }catch(const std::exception& e){
        //大多数情况都不会走这里
        std::string err = "[工具调用失败] " + std::string(e.what());
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, err, sessionId);

        std::cout << "Tool call failed" << std::endl << std::string(e.what());
        return err;
    }
    
    //再次构建结果分析promopt
    std::string secondPrompt = config.buildToolResultPrompt(userQuestion,call.name,call.args,toolJson);

    LOG_DEBUG << "工具调用第二轮提示词"<<secondPrompt;
    //再次放入内存，上下文要一块发给大模型
    messages.push_back({ secondPrompt, 0 });

    //构建上下文请求消息
    json secondRequest = strategy->buildRequest(this->messages);
    // 发送消息
    json secondResponse = executeCurl(secondRequest);
    //解析结果
    std::string finalAnswer = strategy->parseResponse(secondResponse);
    //删除提示词（Server发送的)
    messages.pop_back();

    LOG_DEBUG << "模型最终回答 " << finalAnswer;

    // 保存数据库
    addMessage(userId,userName,true,userQuestion,sessionId);
    addMessage(userId,userName,false,finalAnswer,sessionId);
    return finalAnswer;
}

// 发送自定义请求体
json AIHelper::request(const json &payload)
{
    return executeCurl(payload);
}

//获取内存所有对话信息，前端可能会手动选择获取所有消息
std::vector<std::pair<std::string, long long>> AIHelper::GetMessages()
{
    return this->messages;
}

std::string AIHelper::escapeString(const std::string &input)
{
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '\'': output += "\\\'"; break;
            case '\"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }
    return output;
}

void AIHelper::pushMessageToMysql(int userId, const std::string &userName, bool is_user, const std::string &userInput, long long ms, std::string sessionId)
{
    std::string safeUserName = escapeString(userName);
    std::string safeUserInput = escapeString(userInput);

    std::string sql = "INSERT INTO chat_message (id, username, session_id, is_user, content, ts) VALUES ("
        + std::to_string(userId) + ", "
        + "'" + safeUserName + "', "
        + sessionId + ", "
        + std::to_string(is_user ? 1 : 0) + ", "
        + "'" + safeUserInput + "', "
        + std::to_string(ms) + ")";

    //改成消息队列异步执行mysql操作，用于流量削峰与解耦逻辑，sql_queue路由键
    MQManager::instance().publish("sql_queue", sql);
}

json AIHelper::executeCurl(const json &payload)
{
    CURL* curl = curl_easy_init();
    if(!curl){
        throw std::runtime_error("Failed to initialize curl");
    }

    std::cout<<"curl请求: "<< strategy->getApiUrl().c_str()<<' '<< strategy->getApiKey()<<std::endl;

    std::string readBuffer;
    struct curl_slist* headers = nullptr;
    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey();

    //添加HTTP请求头
    // 身份认证（几乎所有 AI API 都需要）
    headers = curl_slist_append(headers, authHeader.c_str());
    // 告诉服务器请求体是 JSON 格式
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    std::string payloadStr = payload.dump();

    // 这里是请求的链接地址
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    // 附加自定义请求头（认证 + JSON 类型）
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // 设置 POST 请求体（即 payloadStr）
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    // 指定接收响应数据的回调函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    // 传递给回调函数的上下文（这里是 readBuffer 的地址）
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);   // 释放 header 列表
    curl_easy_cleanup(curl);        // 释放 curl 句柄

    if (res != CURLE_OK) {
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    }
    try {
        LOG_INFO<<"模型原始回复"<<readBuffer;
        return json::parse(readBuffer);
    }
    catch (...) {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}

size_t AIHelper::WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}
