#include "AIToolRegistry.h"

AIToolRegistry::AIToolRegistry()
{
    //默认注册自定义的工具
    registerTool("get_weather",getWeather);
    registerTool("get_time",getTime);
}

//可以选择自己注册工具，工具具体调用方法应该由MCP服务器决定，
// MCP服务器帮忙调用注册过的工具，并返回结果
//按理说agent收到用户请求只负责处理MCP服务器、客户、大模型之间的桥梁
//这里实际上又担任了智能体和MCP服务器一部分作用
void AIToolRegistry::registerTool(const std::string &name, ToolFunc func)
{
    tools_[name] = func;
}

// 调用工具
json AIToolRegistry::invoke(const std::string &name, const json &args) const
{
    auto iter = tools_.find(name);
    if(iter==tools_.end()){
        throw std::runtime_error("Tool not find: "+name);
    }
    return iter->second(args);
}

bool AIToolRegistry::hasTool(const std::string &name) const
{
    return tools_.count(name)>0;
}

size_t AIToolRegistry::WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output)
{
    size_t  totalSize = size * nmemb;
    output->append((char*)contents,totalSize);
    return totalSize;
}

json AIToolRegistry::getWeather(const json &args)
{
    if(!args.contains("city")){
        return json{{"error","Missing parameter: city"}};
    }

    std::string city = args["city"].get<std::string>();
    std::string encodedCity;

    // 提取城市名称并 URL 编码
    // 城市名可能包含中文、空格、特殊字符（如 New York、北京）
    // URL 中这些字符必须被编码，否则请求会失败或被服务器拒绝
    char* encoded = curl_easy_escape(nullptr,city.c_str(),city.length());
    if(encoded){
        encodedCity = encoded;
        curl_free(encoded);
    }else{
        return json{{"error","URL encode failed"}};
    }

    std::string url  = "https://wttr.in/" + encodedCity + "?format=3&lang=zh";

    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) {
        return json{ {"error", "Failed to init CURL"} };
    }

    // 注册目标地址
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // 指定接收响应数据的回调函数
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    //传递给回调函数的上下文（这里是 response 字符串地址）
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    // 防止卡死，5 秒超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    // 	自动跟随 301/302 重定向（wttr.in 有时会跳转）
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // curl_easy_perform：同步发起请求（阻塞直到完成）
    CURLcode res = curl_easy_perform(curl);
    // 释放 curl 句柄
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return json{ {"error", "CURL request failed"} };
    }

    
    return json{ {"city", city}, {"weather", response} };
}

//获取本地时间
json AIToolRegistry::getTime(const json &args)
{
    // 作用：显式忽略未使用的参数 args，避免编译器警告
    (void)args;
    // 返回自 Unix 纪元（1970-01-01 00:00:00 UTC） 以来的秒数。
    std::time_t t = std::time(nullptr);
    // 将 time_t 时间戳转换为 本地时区 的结构化时间（struct tm）
    std::tm* now = std::localtime(&t);
    char buffer[64];
    // 格式化时间为字符串
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", now);
    // 初始化列表初始化，构造一个 JSON 对象：{ "time": "2025-04-05 14:30:22" }
    return json{ {"time", buffer} };
}
