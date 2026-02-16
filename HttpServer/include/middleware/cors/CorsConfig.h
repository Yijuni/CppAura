#pragma once

#include <string>
#include <vector>

namespace http 
{
namespace middleware 
{

struct CorsConfig 
{
    std::vector<std::string> allowedOrigins; //允许请求的原
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;
    // 当此头为 true 时，Allow-Origin 不能是 "*"，必须是具体域名
    bool allowCredentials = false;
    int maxAge = 3600;
    
    //返回默认配置
    static CorsConfig defaultConfig() 
    {
        CorsConfig config;
        //允许哪些源访问	https://myapp.com 或 *（不推荐）
        config.allowedOrigins = {"*"};
        //允许的 HTTP 方法
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        //允许的自定义请求头
        config.allowedHeaders = {"Content-Type", "Authorization"};
        return config;
    }
};

} // namespace middleware
} // namespace http