#include "MiddlewareChain.h"
#include <muduo/base/Logging.h>
namespace http 
{
namespace middleware 
{
void http::middleware::MiddlewareChain::addMiddleware(std::shared_ptr<Middleware> middleware)
{
    middlewares_m.push_back(middleware);
}

void http::middleware::MiddlewareChain::processBefore(HttpRequest &request)
{
    //逐个调用即可
    for(auto &middleware : middlewares_m){
        middleware->before(request);
    }
}

// 中间件链的执行顺序遵循“洋葱模型”（Onion Model）：
// 请求阶段（processBefore）：从外到内（正序）
// 响应阶段（processAfter）：从内到外（反序）
void MiddlewareChain::processAfter(HttpResponse &response)
{
    try
    {
        // 反向处理响应，以保持中间件的正确执行顺序
        // rbegin()：返回一个 反向迭代器（reverse iterator），指向容器最后一个元素。
        for (auto it = middlewares_m.rbegin(); it != middlewares_m.rend(); ++it)
        {
            if (*it)
            { // 添加空指针检查
                (*it)->after(response);
            }
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR << "Error in middleware after processing: " << e.what();
    }
}
}
}