#include "../../include/http/HttpRequest.h"
#include "HttpRequest.h"
namespace http
{
    
void http::HttpRequest::setReceiveTime(muduo::Timestamp t)
{
    receiveTime_m = t;
}

bool http::HttpRequest::setMethod(const char *start, const char *end)
{
    assert(method_m==kInvalid);
    std::string m(start,end);  //[start,end)
    if(m=="GET"){
        method_m = kGet;
    }
    else if (m == "POST")
    {
        method_m = kPost;
    }
    else if (m == "PUT")
    {
        method_m = kPut;
    }
    else if (m == "DELETE")
    {
        method_m = kDelete;
    }
    else if (m == "OPTIONS")
    {
        method_m = kOptions;
    }
    else
    {
        method_m = kInvalid;
    }

    return method_m != kInvalid;
}

void http::HttpRequest::setPath(const char *start, const char *end)
{
    path_m.assign(start,end);
}

void http::HttpRequest::setPathParameters(const std::string &key, const std::string &value)
{
    pathParameters_m[key] = value;
}

std::string http::HttpRequest::getPathParameters(const std::string &key) const
{
    auto iter = pathParameters_m.find(key);
    if(iter!=pathParameters_m.end()){
        return iter->second; 
    }
    return "";
}

// 这是从问号后面分割参数
void http::HttpRequest::setQueryParameters(const char *start, const char *end)
{
    std::string argumentStr(start,end);
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

    // 按 & 分割多个参数
    while((pos = argumentStr.find('&',prev)) != std::string::npos){
        std::string pair = argumentStr.substr(prev,pos-prev);
        std::string::size_type equalPos = pair.find('=');

        if(equalPos != std::string::npos){
            std::string key = pair.substr(0,equalPos);
            std::string value = pair.substr(equalPos+1);
            queryParameters_m[key] = value;
        }

        prev = pos+1;
    }

    std::string lastPair = argumentStr.substr(prev);
    std::string::size_type equalPos = lastPair.find('=');
    if(equalPos!=std::string::npos){
        std::string key = lastPair.substr(0,equalPos);
        std::string value = lastPair.substr(equalPos+1);
        queryParameters_m[key] = value;
    }
}

std::string http::HttpRequest::getQueryParameters(const std::string &key) const
{
    auto iter = queryParameters_m.find(key);
    if(iter==queryParameters_m.end()){
        return "";
    }
    return iter->second;
}
// 解析一行 header（如 Content-Type: application/json）
// start 指向字段名开头，colon 指向 :，end 指向行尾
void http::HttpRequest::addHeader(const char *start, const char *colon, const char *end)
{
    std::string key(start,colon);
    while(colon<end && isspace(*colon)){
        ++colon;
    }
    std::string value(colon,end);
    while(!value.empty() && isspace(value[value.size()-1])){
        value.resize(value.size()-1);
    }
    headers_m[key] = value;
}

std::string http::HttpRequest::getHeader(const std::string &field) const
{
    auto iter = headers_m.find(field);
    if(iter==headers_m.end()){
        return "";
    }
    return iter->second;
}

void http::HttpRequest::swap(HttpRequest &that)
{
    std::swap(method_m,that.method_m);
    std::swap(path_m,that.path_m);
    std::swap(pathParameters_m,that.pathParameters_m);
    std::swap(queryParameters_m,that.queryParameters_m);
    std::swap(version_m,that.version_m);
    std::swap(headers_m,that.headers_m);
    std::swap(receiveTime_m,that.receiveTime_m);
}

} //namespace http