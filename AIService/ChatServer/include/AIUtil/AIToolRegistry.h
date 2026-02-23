#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <curl/curl.h>
#include "JsonUtil.h"

class AIToolRegistry {
public:
    //工具回调函数,传入的是工具参数
    using ToolFunc = std::function<json(const json&)>;

    AIToolRegistry();

    void registerTool(const std::string& name, ToolFunc func);
    json invoke(const std::string& name, const json& args) const;
    bool hasTool(const std::string& name) const;

private:
    std::unordered_map<std::string, ToolFunc> tools_;

    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
    static json getWeather(const json& args);
    static json getTime(const json& args);
};