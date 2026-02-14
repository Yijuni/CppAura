#include "HttpContext.h"

namespace http{
//将HTTP报文解析出来封装到HttpRequest对象里面
bool http::HttpContext::parseRequest(muduo::net::Buffer *buf, muduo::Timestamp receiveTime)
{
    bool ok = true; //解析每行请求格式是否正确,格式不正确
    bool hasMore = true;
    while(hasMore){
        if(state_m==kExpectRequestLine){//解析请求行 状态1
            //找到HTTP行结束标志符 \r\n
            const char *crlf = buf->findCRLF(); //找到则返回\r对应的指针否则返回null
            if(crlf){
                ok = processRequestLine(buf->peek(),crlf); //可读数据的起始地址到\r [begin,end)
                if(ok){
                    request_m.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf+2); //调过\r\n,也就是告诉缓冲区从可读位置到crlf全部消费掉 [readindex,\n]都消费掉了
                    state_m = kExpectHeaders;
                }else{
                    hasMore = false; //解析失败退出
                }
            }else{
                hasMore = false; //未找到\r\n，停止解析等待下次回调
            }
        }else if(state_m==kExpectHeaders){ //解析请求头 状态2
            const char *crlf = buf->findCRLF(); //同样的解析每个请求头
            if(crlf){ 
                const char *colon = std::find(buf->peek(),crlf,':');
                if(colon<crlf){//解析普通请求行: Content-Type: application/json
                    //解析字段名和值
                    request_m.addHeader(buf->peek(),colon,crlf);
                }else if(buf->peek()==crlf){ //也就是crlf单独一行，说明，请求头结束了
                    if(request_m.method()==HttpRequest::kPost || request_m.method()==HttpRequest::kPut){
                        std::string contentLength = request_m.getHeader("Content-Length");
                        if(!contentLength.empty()){
                            request_m.setContentLength(std::stoi(contentLength));
                            if(request_m.contentLength()>0){ //存在请求体，继续解析需要
                                state_m = kExpectBody; 
                            }else{ //没有更多数据
                                state_m = kGotAll;
                                hasMore = false;
                            }
                        }else{// POST/PUT 请求没有 Content-Length，是HTTP语法错误
                            ok = false;
                            hasMore = false;
                        }
                    }else{ //GET / HEAD / DELETE 等（无 Body）
                        state_m = kGotAll;
                        hasMore  =false;
                    }
                }else{ //行错误，数据不是请求头的结束，但是也没发现 ":",说明非法 如 "Host example.com"（缺少冒号）→ 非法
                    ok = false;
                    hasMore = false;
                }

                //消费当前请求行(无论哪种情况)
                buf->retrieveUntil(crlf+2);
            }else{ //数据不完整，等待下一次回调
                hasMore = false;
            }
        }else{ //解析请求体，状态3
            //检查缓冲区是否有足够数据,可读数据<请求体长度:数据不完整
            if(buf->readableBytes()<request_m.contentLength()){
                hasMore = false;
            }else{
                //读取指定长度
                std::string body(buf->peek(),buf->peek()+request_m.contentLength());
                request_m.setBody(body);

                //准确的移动指针，不能移动多了
                buf->retrieve(request_m.contentLength());
                state_m = kGotAll;
                hasMore = false;
            }
        }
    } 
    return ok;
}

//解析请求行用
bool http::HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed =false;
    const char* start = begin;
    const char* space = std::find(begin,end,' ');

    if(space!=end && request_m.setMethod(start,space)){
        start = space+1;
        space = std::find(start,end,' ');
        if(space!=end){
            const char *argumentStart = std::find(start,space,'?');
            if(argumentStart!=space){ //请求带参数
                request_m.setPath(start,argumentStart);
                request_m.setQueryParameters(argumentStart+1,space);
            }else{
                request_m.setPath(start,space);
            }

            start = space+1;
            succeed = ((end-start == 8) && std::equal(start,end-1,"HTTP/1."));
            if(succeed){
                if(*(end-1) == '1'){
                    request_m.setVersion("HTTP/1.1");
                }else if(*(end-1)=='0'){
                    request_m.setVersion("Http/1.0");
                }else{
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}
} //namespace http
