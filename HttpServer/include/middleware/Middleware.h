#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace http
{ 
namespace middleware
{

//定义中间件的抽象接口
class Middleware{
public:
    virtual ~Middleware() = default;

    //请求前处理
    virtual void before(HttpRequest& request) = 0;

    //请求后处理
    virtual void after(HttpResponse& response) = 0;
    
};

}
}