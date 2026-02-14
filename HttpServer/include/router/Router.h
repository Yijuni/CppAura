#pragma once
#include <iostream>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <regex>
#include <vector>

#include "RouterHandler.h"
#include "../http/HttpRequest.h"
#include "../http/HttpResponse.h"

namespace http
{
namespace router
{

// 选择注册"对象式的路由处理器" 还是注册 "回调函数式的处理器" 取决于处理器执行的复杂程度
// 简单的处理 注册回调函数
// 复杂的处理 注册对象式路由处理器(对象中可封装多个相关函数，涉及多个步骤)
// 二者注册其一即可
class Router
{
public:
    using HandlerPtr = std::shared_ptr<RouterHandler>;
    using HandlerCallback = std::function<void(const HttpRequest &, HttpResponse *)>;

    // 路由键（请求方法 + URI）
    struct RouteKey
    {
        HttpRequest::Method method;
        std::string path;

        //重载 == 为 unordered_map 提供相等比较，(hash冲突的时候用来比较)
        bool operator==(const RouteKey &other) const
        {
            return method == other.method && path == other.path;
        }
    };

    // 为RouteKey 定义哈希函数
    struct RouteKeyHash
    {
        // 为 RouteKey 提供仿函数哈希算法(因为底层是hash表)，使其能作为 unordered_map 的 key
        size_t operator()(const RouteKey &key) const
        {
            size_t methodHash = std::hash<int>{}(static_cast<int>(key.method));
            size_t pathHash = std::hash<std::string>{}(key.path);
            return methodHash * 31 + pathHash;
        }
    };

    // 注册路由处理器
    void registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler);

    // 注册回调函数形式的处理器
    void registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback);

    //对于满足某种特定路径的请求，可以使用同一个回调函数
    // 注册动态路由处理器
    void addRegexHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
    {
        std::regex pathRegex = convertToRegex(path);
        regexhandlers_m.emplace_back(method, pathRegex, handler);
    }

    // 注册动态路由处理函数
    void addRegexCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
    {
        std::regex pathRegex = convertToRegex(path);
        regexcallbacks_m.emplace_back(method, pathRegex, callback);
    }

    // 处理请求
    bool route(const HttpRequest &req, HttpResponse *resp);

private:
    // 将路径模式转换为正则表达式，支持匹配任意路径参数
    // 输入："/user/:id/profile"
    // 输出正则：^/user/([^/]+)/profile$
    // 使用 原始字符串字面量 R"(...)" 避免转义地狱
    // : 后跟非 / 字符 → 捕获组 ([^/]+)
    // 支持多参数：/post/:year/:month → ^/post/([^/]+)/([^/]+)$
    std::regex convertToRegex(const std::string &pathPattern)
    { 
        std::string regexPattern = "^" + std::regex_replace(pathPattern, std::regex(R"(/:([^/]+))"), R"(/([^/]+))") + "$";
        return std::regex(regexPattern);
    }

    // 提取路径参数
    // 将正则匹配的捕获组（match[1], match[2]...）存入 HttpRequest
    // 键名为 "param1", "param2"...（可改进为使用原参数名如 "id"）
    void extractPathParameters(const std::smatch &match, HttpRequest &request)
    {
        // Assuming the first match is the full path, parameters start from index 1
        for (size_t i = 1; i < match.size(); ++i)
        {
            request.setPathParameters("param" + std::to_string(i), match[i].str());
        }
    }

private:
    struct RouteCallbackObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerCallback callback_;
        RouteCallbackObj(HttpRequest::Method method, std::regex pathRegex, const HandlerCallback &callback)
            : method_(method), pathRegex_(pathRegex), callback_(callback) {}
    };

    struct RouteHandlerObj
    {
        HttpRequest::Method method_;
        std::regex pathRegex_;
        HandlerPtr handler_;
        RouteHandlerObj(HttpRequest::Method method, std::regex pathRegex, HandlerPtr handler)
            : method_(method), pathRegex_(pathRegex), handler_(handler) {}
    };

    std::unordered_map<RouteKey, HandlerPtr, RouteKeyHash>      handlers_m;       // 精准匹配
    std::unordered_map<RouteKey, HandlerCallback, RouteKeyHash> callbacks_m; // 精准匹配

    std::vector<RouteHandlerObj>                                regexhandlers_m;     // 正则匹配
    std::vector<RouteCallbackObj>                               regexcallbacks_m;   // 正则匹配
};


} // namespace router
} // namespace http