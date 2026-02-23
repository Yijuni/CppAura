#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <fstream>
#include <sstream>
#include <iostream>
#include "JsonUtil.h"  

//可用工具
struct AITool{
    std::string name;
    std::unordered_map<std::string,std::string> params;
    std::string desc;
};

//模型回复解析
struct AIToolCall{
    std::string name;
    json args;
    bool isToolCall = false;
};
//一般流程 loadFromFile(初始化工具)--->
// buildPrompt(构建请求发送给模型)-->parseAIResponse(解析模型回复)->
// buildToolResultPrompt(构建请求结果再次发送给模型)
class AIConfig{
public:
    bool loadFromFile(const std::string& path);
    std::string buildPrompt(const std::string& userInput) const;
    AIToolCall parseAIResponse(const std::string& response) const;
    std::string buildToolResultPrompt(const std::string& userInput,const std::string& toolName,const json& toolArgs,const json& toolResult) const;
private:
    std::string promptTemplate_;
    std::vector<AITool> tools_;

    std::string buildToolList() const;
};