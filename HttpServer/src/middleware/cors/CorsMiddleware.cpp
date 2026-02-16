#include "CorsMiddleware.h"
#include <muduo/base/Logging.h>
#include <iostream>
#include <sstream>
#include <algorithm>
namespace http 
{
namespace middleware 
{
http::middleware::CorsMiddleware::CorsMiddleware(const CorsConfig &config):config_m(config)
{

}

void http::middleware::CorsMiddleware::before(HttpRequest &request)
{
    LOG_DEBUG << "CorsMiddleware::before - Processing request";

    //专门处理浏览器自动发起的 OPTIONS 预检请求（Preflight Request）
    if(request.method()==http::HttpRequest::kOptions){
        LOG_INFO<<"Processing CORS preflight request";
        HttpResponse response;
        handlePreflightRequest(request,response);
        
        // 这不是错误！而是一种“短路响应”机制。
        // 通过抛出 HttpResponse 对象，立即终止后续中间件和路由处理，直接返回这个响应给浏览器。
        //抛出之后会被外部的catch捕获，然后执行相应逻辑
        throw response;
    }
}

void http::middleware::CorsMiddleware::after(HttpResponse &response)
{
    LOG_DEBUG << "CorsMiddleware::after - Processing response";
    
    // 直接添加CORS头，简化处理逻辑
    if (!config_m.allowedOrigins.empty()) 
    {
        // 如果允许所有源
        if (std::find(config_m.allowedOrigins.begin(), config_m.allowedOrigins.end(), "*") 
            != config_m.allowedOrigins.end()) 
        {
            addCorsHeaders(response, "*");
        } 
        else 
        {
            //后续可能会要修改这部分
            // 添加第一个允许的源
            addCorsHeaders(response, config_m.allowedOrigins[0]);
        }
    }
}

std::string http::middleware::CorsMiddleware::join(const std::vector<std::string> &strings, const std::string &delimiter)
{
    std::ostringstream result;
    for(size_t i=0;i<strings.size();++i){
        if(i>0) result<<delimiter;
        result<<strings[i];
    }
    return result.str();
}

bool http::middleware::CorsMiddleware::isOriginAllowed(const std::string &origin) const
{
    return config_m.allowedOrigins.empty() 
        || std::find(config_m.allowedOrigins.begin(),config_m.allowedOrigins.end(),"*") != config_m.allowedOrigins.end()
        || std::find(config_m.allowedOrigins.begin(),config_m.allowedOrigins.end(),origin) != config_m.allowedOrigins.end();
}

void http::middleware::CorsMiddleware::handlePreflightRequest(const HttpRequest &request, HttpResponse &response)
{
    //判断请求源是否被允许
    const std::string origin = request.getHeader("Origin");
    if(!isOriginAllowed(origin)){
        LOG_WARN << "Origin not allowed: "<< origin;
        //“服务器理解你的请求，但拒绝执行它。”
        response.setStatusCode(HttpResponse::k403Forbidden);
        return;
    }

    addCorsHeaders(response,origin);
    // 请求已成功处理，但服务器不需要返回任何内容。
    response.setStatusCode(HttpResponse::k204NoContent);
    LOG_INFO << "Preflight request processed successfully";
}

void http::middleware::CorsMiddleware::addCorsHeaders(HttpResponse &response, const std::string &origin)
{
    try{
        response.addHeader("Access-Control-Allow-Origin", origin);

        // 控制是否允许前端携带凭据（如 Cookies、HTTP 认证、Authorization 头）
        if(config_m.allowCredentials){
            // 关键限制：当此头为 true 时，Allow-Origin 不能是 "*"，必须是具体域名
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }
        if (!config_m.allowedMethods.empty()) 
        {
            // 这个头告诉浏览器：“这些方法被允许用于跨域请求”
            response.addHeader("Access-Control-Allow-Methods", 
                             join(config_m.allowedMethods, ", "));
        }
        if (!config_m.allowedHeaders.empty()) 
        {
            // 允许前端发送的自定义请求头（如 "Content-Type", "Authorization"）。
            // 如果前端请求包含未在此列出的头（如 X-API-Key），浏览器会 block 请求
            response.addHeader("Access-Control-Allow-Headers", 
                             join(config_m.allowedHeaders, ", "));
        }
        // Max-Age 表示预检请求结果的缓存时间（秒）。
        // 例如 maxAge = 3600 → 浏览器在 1 小时内不再发送 OPTIONS 请求，直接发真实请求。
        response.addHeader("Access-Control-Max-Age", std::to_string(config_m.maxAge));
        LOG_DEBUG << "CORS headers added successfully";
    }catch (const std::exception& e ){
        LOG_ERROR << "Error adding CORS headers: " << e.what();
    }
}
}
}