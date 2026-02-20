#pragma once
#include "SslContext.h"
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/noncopyable.h>
#include <openssl/ssl.h>
#include <memory>
/*                        加密数据           加密数据         解密数据
    消息接收流程 muduo_buf -------> read_bio ------> ssl_read -------> muduo_buf
    消息发送流程 
*/
namespace ssl
{
// Muduo 只看到加密数据，业务只看到明文数据，SslConnection 在中间做转换。
// 添加消息回调函数类型定义
using MessageCallback = std::function<void(const std::shared_ptr<muduo::net::TcpConnection>&,
                                         muduo::net::Buffer*,
                                         muduo::Timestamp)>;

class SslConnection : muduo::noncopyable 
{
public:
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    using BufferPtr = muduo::net::Buffer*;
    
    SslConnection(const TcpConnectionPtr& conn, SslContext* ctx);
    ~SslConnection();

    void startHandshake();
    void send(const void* data, size_t len);
    void onRead(const TcpConnectionPtr& conn, BufferPtr buf, muduo::Timestamp time);
    bool isHandshakeCompleted() const { return state_m == SSLState::ESTABLISHED; }
    muduo::net::Buffer* getDecryptedBuffer() { return &decryptedBuffer_m; }
    // SSL BIO 操作回调
    static int bioWrite(BIO* bio, const char* data, int len);
    static int bioRead(BIO* bio, char* data, int len);
    static long bioCtrl(BIO* bio, int cmd, long num, void* ptr);
    // 设置消息回调函数
    void setMessageCallback(const MessageCallback& cb) { messageCallback_m = cb; }
private:
    void handleHandshake();
    void onEncrypted(const char* data, size_t len);
    void onDecrypted(const char* data, size_t len);
    SSLError getLastError(int ret);
    void handleError(SSLError error);

private:
    SSL*                ssl_m; // SSL 连接
    SslContext*         ctx_m; // SSL 上下文
    TcpConnectionPtr    conn_m; // TCP 连接
    SSLState            state_m; // SSL 状态
    BIO*                readBio_m;   // 网络数据 -> SSL
    BIO*                writeBio_m;  // SSL -> 网络数据
    muduo::net::Buffer  readBuffer_m; // 读缓冲区
    muduo::net::Buffer  writeBuffer_m; // 写缓冲区
    muduo::net::Buffer  decryptedBuffer_m; // 解密后的数据
    MessageCallback     messageCallback_m; // 消息回调,解密完成后要调用的回调函数
};

}