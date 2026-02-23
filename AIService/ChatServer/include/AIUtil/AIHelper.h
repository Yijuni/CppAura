#pragma once
#include <string>
#include <vector>
#include <utility>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

#include "JsonUtil.h"
#include"MysqlUtil.h"

#include"AIFactory.h"
#include"AIConfig.h"
#include"AIToolRegistry.h"

/**
 * 封装curl对阿里云百炼的访问
 */

class AIHelper{
public:
    //构造函数,初始化API-key
    AIHelper();

    // 设计模式体现：使用 策略模式（Strategy Pattern）
    // 将具体 AI 行为（如调用 Qwen、GLM、Claude 等）抽象为 AIStrategy 接口
    void setStrategy(std::shared_ptr<AIStrategy> strat);

    // 添加用户消息到本地数据库
    /// @brief 
    /// @param userId 用户id
    /// @param userName 用户姓名
    /// @param is_user 是不是用户消息
    /// @param userInput 用户输入
    /// @param sessionId 会话id，多轮对话相互隔离的
    void addMessage(int userId, const std::string& userName, bool is_user, 
        const std::string& userInput, std::string sessionId);

    //这里就是服务器启动时重新加载数据到内存
    void restorMessage(const std::string& userInput, long long ms);
    
    // 发送聊天消息，返回AI的响应内容
    // messages: [{"role":"system","content":"..."}, {"role":"user","content":"..."}]
    std::string chat(int userId, std::string userName, std::string sessionId, std::string userQuestion, std::string modelType);

    // 可选：发送自定义请求体
    // 允许上层传入自定义 JSON 请求体，灵活性高
    // 返回原始 JSON 响应（供高级用户解析）
    json request(const json& payload);
    
    // 返回当前会话的所有消息（内容 + 时间戳）
    std::vector<std::pair<std::string, long long>> GetMessages();

private:
    std::string escapeString(const std::string& input);
    //加入到mysql的接口（提供加入到线程池的接口，线程池做异步mysql更新操作）
    //todo: 
    void pushMessageToMysql(int userId, const std::string& userName, bool is_user, const std::string& userInput, long long ms,std::string sessionId);

    // 内部方法：执行curl请求，返回原始JSON
    json executeCurl(const json& payload);
    // curl 回调函数，把返回的数据写到 string buffer
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    //策略模型，具体策略根据初始化选择
    std::shared_ptr<AIStrategy> strategy;

    //一个用户针对一个AIHelper，messages存放用户的历史对话
    //偶数下标代表用户的信息，奇数下标是ai返回的内容
    //后者代表时间戳
    std::vector<std::pair<std::string, long long>> messages;
};