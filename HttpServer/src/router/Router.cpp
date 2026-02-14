#include "Router.h"
namespace http
{
namespace router
{
void http::router::Router::registerHandler(HttpRequest::Method method, const std::string &path, HandlerPtr handler)
{
    RouteKey key{method,path};
    handlers_m[key] = std::move(handler); 
}

void http::router::Router::registerCallback(HttpRequest::Method method, const std::string &path, const HandlerCallback &callback)
{
    RouteKey key{method, path};
    callbacks_m[key] = std::move(callback);
}

bool http::router::Router::route(const HttpRequest &req, HttpResponse *resp)
{
    RouteKey key{req.method(),req.path()};

    //查找处理器
    auto handerIt = handlers_m.find(key);
    if(handerIt != handlers_m.end()){
        handerIt->second->handle(req,resp);
        return true;
    }

    //查找回调函数
    auto callbackIt = callbacks_m.find(key);
    if(callbackIt != callbacks_m.end()){
        callbackIt->second(req,resp);
        return true;
    }

    //查找动态对象路由处理器
    for(const auto &[method,pathRegex,handler] : regexhandlers_m){
        std::smatch match;
        std::string pathStr(req.path());
        //方法匹配而且动态路由可以配得上
        if(method==req.method() && std::regex_match(pathStr,match,pathRegex)){
            HttpRequest newReq(req); //req参数不可改变，只能拷贝构造一份
            extractPathParameters(match,newReq);

            handler->handle(newReq,resp);
            return true;
        }
    }

    //查找动态回调函数处理器
    for(const auto &[method,pathRegex,callback] : regexcallbacks_m){
        std::smatch match;
        std::string pathStr(req.path());
        if(method==req.method() && std::regex_match(pathStr,match,pathRegex)){
            HttpRequest newReq(req);
            extractPathParameters(match,newReq);

            callback(newReq,resp);
            return true;
        }
    }
    return false;
}
} //namespace router
} //namespace http
