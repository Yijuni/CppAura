#pragma once

#include <muduo/net/TcpServer.h>

namespace http
{

class HttpResponse 
{
public:
    enum HttpStatusCode
    {
        kUnknown,
        k200Ok = 200, //成功处理请求
        k204NoContent = 204, //请求成功，但无响应体
        k301MovedPermanently = 301, //永久重定向
        k400BadRequest = 400, //请求语法错误或参数无效
        k401Unauthorized = 401,//需要身份验证（未登录）
        k403Forbidden = 403,//服务器拒绝请求（权限不足）
        k404NotFound = 404, //请求的资源不存在
        k409Conflict = 409,//请求与资源当前状态冲突
        k500InternalServerError = 500, //服务器内部出错
    };

    HttpResponse(bool close = true)
        : statusCode_m(kUnknown)
        , closeConnection_m(close),
        httpVersion_m("HTTP/1.0")
    {}

    void setVersion(std::string version)
    { httpVersion_m = version; }
    void setStatusCode(HttpStatusCode code)
    { statusCode_m = code; }

    HttpStatusCode getStatusCode() const
    { return statusCode_m; }

    void setStatusMessage(const std::string message)
    { statusMessage_m = message; }

    void setCloseConnection(bool on)
    { closeConnection_m = on; }

    bool closeConnection() const
    { return closeConnection_m; }
    
    void setContentType(const std::string& contentType)
    { addHeader("Content-Type", contentType); }

    void setContentLength(uint64_t length)
    { addHeader("Content-Length", std::to_string(length)); }

    void addHeader(const std::string& key, const std::string& value)
    { headers_m[key] = value; }
    
    void setBody(const std::string& body)
    { 
        body_m = body;
        setContentLength(body_m.size());
    }

    void setStatusLine(const std::string& version,
                         HttpStatusCode statusCode,
                         const std::string& statusMessage);

    void setErrorHeader(){}

    void appendToBuffer(muduo::net::Buffer* outputBuf) const;
private:
    std::string                        httpVersion_m; 
    HttpStatusCode                     statusCode_m;
    std::string                        statusMessage_m;
    bool                               closeConnection_m;
    std::map<std::string, std::string> headers_m;
    std::string                        body_m;
    bool                               isFile_m;
};

} // namespace http