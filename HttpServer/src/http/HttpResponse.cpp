#include "http/HttpResponse.h"

namespace http
{

void http::HttpResponse::setStatusLine(const std::string &version, HttpStatusCode statusCode, const std::string &statusMessage)
{
    httpVersion_m = version;
    statusCode_m = statusCode;
    statusMessage_m = statusMessage;
}

void http::HttpResponse::appendToBuffer(muduo::net::Buffer *outputBuf) const
{
    //HttpResponse封装的信息格式化输出
    char buf[32];
    
    // 为什么不把状态信息一块放入格式化字符串中，因为状态信息有长有短，不方便定义一个固定大小的内存存储
    snprintf(buf,sizeof buf,"%s %d " ,httpVersion_m.c_str(),statusCode_m);

    //组合响应行
    outputBuf->append(buf);
    outputBuf->append(statusMessage_m);
    outputBuf->append("\r\n");

    //组合响应头
    if(closeConnection_m){
        outputBuf->append("Connection: close\r\n");
    }else{
        outputBuf->append("Connection: Keep-Alive\r\n");
    }
    for(const auto& header: headers_m){
        //响应头key value长度不固定，不能用格式化输出
        //对不定长字符串，直接 append，避免中间缓冲区
        outputBuf->append(header.first);
        outputBuf->append(": ");
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }
    outputBuf->append("\r\n");

    //组合响应体
    outputBuf->append(body_m);
}

} //namespace http