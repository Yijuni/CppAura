#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include <muduo/base/Timestamp.h>

namespace http
{
class HttpRequest
{
public:
    enum Method
    {
        kInvalid,
        kGet,
        kPost,
        kHead,
        kPut,
        kDelete,
        kOptions
    };

    HttpRequest():method_m(kInvalid),version_m("UnKnown"){}

    // 记录该请求被服务器接收到的精确时间（通常来自 muduo 的 Channel 回调）
    void setReceiveTime(muduo::Timestamp t); 
    muduo::Timestamp receiveTime() const { return receiveTime_m; }

    //（如 "GET /index.html HTTP/1.1")
    // 设置 HTTP 请求方法
    bool setMethod(const char* start, const char* end);
    Method method() const { return method_m; }

    //设置 HTTP 请求的路径
    void setPath(const char* start, const char* end);
    std::string path() const { return path_m; }

    //设置动态路由参数，动态匹配   https://api.example.com/users/123 → 123 是路径参数
    //符合 RESTful 规范
    // REST 强调“资源”概念，/users/1001 表示“ID 为 1001 的用户资源”，语义清晰。
    // 接口简洁统一
    // 一个模板处理无限资源，无需为每个 ID 写新接口。
    void setPathParameters(const std::string &key, const std::string &value);
    std::string getPathParameters(const std::string &key) const;

    //设置查询参数 ,解析 URL 中 ? 后的部分（如 ?page=2&size=10）
    void setQueryParameters(const char* start, const char* end);
    std::string getQueryParameters(const std::string &key) const;
    
    //设置版本号,从请求行末尾提取（如 HTTP/1.1）
    void setVersion(std::string v)
    {
        version_m = v;
    }
    //返回版本号
    std::string getVersion() const
    {
        return version_m;
    }

    //添加请求头
    void addHeader(const char* start, const char* colon, const char* end);
    std::string getHeader(const std::string& field) const;
    //获取所有请求头
    const std::map<std::string, std::string>& headers() const
    { return headers_m; }

    //设置请求体内容
    void setBody(const std::string& body) { content_m = body; }
    void setBody(const char* start, const char* end) 
    { 
        if (end >= start) 
        {
            content_m.assign(start, end - start); 
        }
    }
    
    //获取请求体
    std::string getBody() const
    { return content_m; }

    //设置请求体长度
    void setContentLength(uint64_t length)
    { contentLength_m = length; }
    uint64_t contentLength() const
    { return contentLength_m; }

    //交换两个Http请求
    void swap(HttpRequest& that);
private:
    Method method_m; // 请求方法
    std::string path_m; //请求路径
    std::string version_m; //http版本
    std::map<std::string, std::string>           headers_m; // 请求头
    std::string                                  content_m; // 请求体
    std::unordered_map<std::string, std::string> pathParameters_m; // 路径参数
    std::unordered_map<std::string, std::string> queryParameters_m; // 查询参数
    muduo::Timestamp                             receiveTime_m; // 接收时间
    uint64_t                                     contentLength_m { 0 }; // 请求体长度
};

} // namespace http
