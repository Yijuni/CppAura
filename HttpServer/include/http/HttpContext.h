#pragma once

#include <iostream>

#include <muduo/net/TcpServer.h>

#include "HttpRequest.h"

namespace http{

//解析HttpRequest内容并填入对象
class HttpContext 
{
public:
    enum HttpRequestParseState //解析进展，有限状态机标记解析到哪部分了
    {
        kExpectRequestLine, // 解析请求行
        kExpectHeaders, // 解析请求头
        kExpectBody, // 解析请求体
        kGotAll, // 解析完成
    };
    
    HttpContext()
    : state_m(kExpectRequestLine)
    {}

    bool parseRequest(muduo::net::Buffer* buf, muduo::Timestamp receiveTime);
    bool gotAll() const 
    { return state_m == kGotAll;  }

    //开启新一轮的解析，一般是gotAll返回true之后，处理完前一个请求，就调用这个请求
    void reset()
    {
        state_m = kExpectRequestLine;
        HttpRequest dummyData;
        request_m.swap(dummyData);
    }

    const HttpRequest& request() const
    { return request_m;}

    HttpRequest& request()
    { return request_m;}

private:
    bool processRequestLine(const char* begin, const char* end);
private:
    HttpRequestParseState state_m;
    HttpRequest           request_m;
};

} // namespace http