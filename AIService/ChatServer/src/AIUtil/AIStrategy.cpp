#include "AIStrategy.h"
#include"AIFactory.h"

//只注册一次静态的
static StrategyRegister<AliyunStrategy> regAliyun("1");
static StrategyRegister<DouBaoStrategy> regDoubao("2");
static StrategyRegister<AliyunRAGStrategy> regAliyunRag("3");
static StrategyRegister<AliyunMcpStrategy> regAliyunMcp("4");

std::string AliyunStrategy::getApiUrl() const
{
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunStrategy::getApiKey() const
{
    return apiKey_;
}

std::string AliyunStrategy::getModel() const
{
    return "qwen-plus";
}

json AliyunStrategy::buildRequest(const std::vector<std::pair<std::string, long long>> &messages) const
{
    json payload;
    payload["model"] = getModel();
    json msgArry = json::array();
    for(int i=0;i<messages.size();i++){
        json msg;
        if(i%2){
            msg["role"] = "assistant";
        }else{
            msg["role"] = "user";
        }
        msg["content"] = messages[i].first;
        msgArry.push_back(msg);
    }
    payload["messages"] = msgArry;
    return payload; 
}

std::string AliyunStrategy::parseResponse(const json &response) const
{
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

std::string DouBaoStrategy::getApiUrl()const {
    return "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}

std::string DouBaoStrategy::getApiKey()const {
    return apiKey_;
}

std::string DouBaoStrategy::getModel() const {
    return "doubao-seed-2-0-pro-260215";
}

json DouBaoStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    for (size_t i = 0; i < messages.size(); ++i) {
        json msg;
        if (i % 2 == 0) {
            msg["role"] = "user";
        }
        else {
            msg["role"] = "assistant";
        }
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }
    payload["messages"] = msgArray;
    return payload;
}


std::string DouBaoStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}

std::string AliyunRAGStrategy::getApiUrl() const {
    const char* key = std::getenv("Knowledge_Base_ID");
    if (!key) throw std::runtime_error("Knowledge_Base_ID not found!");
    std::string id(key);
    //϶Ӧ֪ʶID
    return "https://dashscope.aliyuncs.com/api/v1/apps/"+id+"/completion";
}

std::string AliyunRAGStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunRAGStrategy::getModel() const {
    return ""; //Ҫģ
}


//阿里百炼的智能体请求和其他的有些不同需要参考官方文档，具体来说，智能体历史对话只传用户的
json AliyunRAGStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    json msgArray = json::array();
    // 最新用户输入不封装
    for (size_t i = 0; i < messages.size()-1; ++i) {
        if(i%2){
            continue; //不放AI回答
        }
        json msg;
        msg["role"] = "user";
        msg["content"] = messages[i].first;
        msgArray.push_back(msg);
    }

    payload["input"]["prompt"] = messages[messages.size()-1];
    payload["input"]["messages"] = msgArray;
    payload["parameters"] = json::object(); 

    // 新版改版，似乎没法自己维护上下文，阿里智能体
    /**
     * {
        "input": {
            "messages": [
            {"role": "user", "content": "你好"}
            ]
        },
        "parameters": {} 
        }
     */
    return payload;
}


std::string AliyunRAGStrategy::parseResponse(const json& response) const {
    if (response.contains("output") && response["output"].contains("text")) {
        return response["output"]["text"];
    }
    return {};
}



std::string AliyunMcpStrategy::getApiUrl() const {
    return "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
}

std::string AliyunMcpStrategy::getApiKey()const {
    return apiKey_;
}


std::string AliyunMcpStrategy::getModel() const {
    return "qwen-plus";
}


json AliyunMcpStrategy::buildRequest(const std::vector<std::pair<std::string, long long>>& messages) const {
    json payload;
    payload["model"] = getModel();
    json msgArray = json::array();

    // for (size_t i = 0; i < messages.size(); ++i) {
    //     json msg;
    //     if (i % 2 == 0) {
    //         msg["role"] = "user";
    //     }
    //     else {
    //         msg["role"] = "assistant";
    //     }
    //     msg["content"] = messages[i].first;
    //     msgArray.push_back(msg);
    // }
    json msg;
    msg["role"] = "user";
    msg["content"] = messages[messages.size()-1].first;
    msgArray.push_back(msg);
    payload["messages"] = msgArray;
    return payload;
}


std::string AliyunMcpStrategy::parseResponse(const json& response) const {
    if (response.contains("choices") && !response["choices"].empty()) {
        return response["choices"][0]["message"]["content"];
    }
    return {};
}