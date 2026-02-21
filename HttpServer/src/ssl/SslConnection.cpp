#include "SslConnection.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>
namespace ssl
{

// 自定义 BIO 方法,Ssl会自己调用读写操作,但是这个代码并没有使用
static BIO_METHOD* createCustomBioMethod() 
{
    BIO_METHOD* method = BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl);
    return method;
}

SslConnection::SslConnection(const TcpConnectionPtr &conn, SslContext *ctx)
    :ssl_m(nullptr),ctx_m(ctx),conn_m(conn),
    state_m(SSLState::HANDSHAKE),readBio_m(nullptr),writeBio_m(nullptr),
    messageCallback_m(nullptr)
{   
    // 创建 SSL 对象
    ssl_m = SSL_new(ctx_m->getNativeHandle());
    if (!ssl_m) {
        LOG_ERROR << "Failed to create SSL object: " << ERR_error_string(ERR_get_error(), nullptr);
        return;
    }

    //创建BIO
    readBio_m = BIO_new(BIO_s_mem());
    writeBio_m = BIO_new(BIO_s_mem());

    if (!readBio_m || !writeBio_m) {
        LOG_ERROR << "Failed to create BIO objects";
        SSL_free(ssl_m);
        ssl_m = nullptr;
        return;
    }

    //ssl后面读数据都会读readBio_m，写数据都会写writeBio_m
    SSL_set_bio(ssl_m, readBio_m, writeBio_m);
    SSL_set_accept_state(ssl_m);  // 设置为服务器模式

    // 设置 SSL 选项
    // 允许传入的 buffer 地址变化（Muduo Buffer 可能 realloc）
    SSL_set_mode(ssl_m, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    // 允许 SSL_write 只写入部分数据（符合异步模型）
    SSL_set_mode(ssl_m, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // 设置连接回调
    conn_m->setMessageCallback(
        std::bind(&SslConnection::onRead, this, std::placeholders::_1,
                 std::placeholders::_2, std::placeholders::_3));
}

SslConnection::~SslConnection()
{
    if (ssl_m) 
    {
        SSL_free(ssl_m);  // 这会同时释放 BIO
    }
}

void ssl::SslConnection::startHandshake()
{
    SSL_set_accept_state(ssl_m);
    handleHandshake();
}

void SslConnection::send(const void *data, size_t len)
{
    if (state_m != SSLState::ESTABLISHED) {
        LOG_ERROR << "Cannot send data before SSL handshake is complete";
        return;
    }

    const char* ptr = static_cast<const char*>(data);
    size_t remain = len;
    // 处理 SSL_write 的部分写入(bio缓冲区一次可能没法全都放下),循环发送直至发送完成
    while(remain>0){
        //会加密数据，然后写入write_bio
        int written = SSL_write(ssl_m, ptr, remain);
        if (written <= 0) {
            int err = SSL_get_error(ssl_m, written);
            LOG_ERROR << "SSL_write failed: " << ERR_error_string(err, nullptr);
            return;
        }
        
        remain -= written;
        ptr += written;

        char buf[4096];
        int pending;
        // BIO_pending(bio) 返回 bio 中可立即读取的字节数（对于 Memory BIO，就是内部缓冲区的数据量）
        // 轮询读取，因为一次可能没法读完
        while ((pending = BIO_pending(writeBio_m)) > 0) {
            //从write_bio读出加密数据，然后发送
            // 每次读取限制数量的数据到缓冲区
            int bytes = BIO_read(writeBio_m, buf, 
                            std::min(pending, static_cast<int>(sizeof(buf))));
            if (bytes > 0) {
                conn_m->send(buf, bytes);
            }else{
                break;
            }
        }
    }

}
void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, 
                         muduo::Timestamp time) 
{
    // 1. 把所有收到的加密数据喂给 OpenSSL,也就是写入bio
    if (buf->readableBytes() > 0) {
        BIO_write(readBio_m, buf->peek(), buf->readableBytes());
        buf->retrieveAll(); // 清空缓冲区
    }
    //2. 处理握手或者连接消息
    if (state_m == SSLState::HANDSHAKE) {
        // BIO属于当前的ssl连接，
        handleHandshake();
    } else if (state_m == SSLState::ESTABLISHED) {//已经建立连接的阶段
        // 3. 循环读取所有可解密数据
        char decryptedData[4096];
        int ret;
        while ((ret = SSL_read(ssl_m, decryptedData, sizeof(decryptedData))) > 0) {
            // 4. 存入成员变量（供回调函数使用）
            decryptedBuffer_m.append(decryptedData, ret);
        }

        // 5. 检查错误
        if (ret < 0) {
            int sslErr = SSL_get_error(ssl_m, ret);
            if (sslErr != SSL_ERROR_WANT_READ) {
                handleError(getLastError(ret));
            }
        }

        // 6. 触发业务回调（传成员变量地址）
        if (messageCallback_m && decryptedBuffer_m.readableBytes() > 0) {
            messageCallback_m(conn, &decryptedBuffer_m, time);
            // 注意：不要在这里 retrieveAll()！由上层决定何时消费
        }
    }
}

//以下三个函数没用到
int SslConnection::bioWrite(BIO* bio, const char* data, int len) 
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    conn->conn_m->send(data, len);
    return len;
}

int SslConnection::bioRead(BIO* bio, char* data, int len) 
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    size_t readable = conn->readBuffer_m.readableBytes();
    if (readable == 0) 
    {
        return -1;  // 无数据可读
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_m.peek(), toRead);
    conn->readBuffer_m.retrieve(toRead);
    return toRead;
}

long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr) 
{
    switch (cmd) 
    {
        case BIO_CTRL_FLUSH:
            return 1;
        default:
            return 0;
    }
}

void SslConnection::handleHandshake()
{
    // SSL_do_handshake 会从你写入的 
    // read-BIO 读数据并把要发送的数据写到 write-BIO
    // 然后返回 SSL_ERROR_WANT_WRITE，这样继续给客户端发送
    // 我想要发送的握手数据
    int ret = SSL_do_handshake(ssl_m);
    
    if (ret == 1) {
        state_m = SSLState::ESTABLISHED;
        LOG_INFO << "SSL handshake completed successfully";
        LOG_INFO << "Using cipher: " << SSL_get_cipher(ssl_m);
        LOG_INFO << "Protocol version: " << SSL_get_version(ssl_m);
        
        // 握手完成后，确保设置了正确的回调
        if (!messageCallback_m) {
            LOG_WARN << "No message callback set after SSL handshake";
        }
        return;
    }
    
    int err = SSL_get_error(ssl_m, ret);
    switch (err) {
        // 等待更多网络数据，正常
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:{
            // 正常的握手过程，需要继续
            // 关键：OpenSSL 有数据要发送！可能服务器主动协商重发
            char outBuf[4096];
            int n;
            while ((n = BIO_read(writeBio_m, outBuf, sizeof(outBuf))) > 0) {
                conn_m->send(outBuf, n); // 发送到网络
            }
            break;
        }
        default: {
            // 获取详细的错误信息
            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            LOG_ERROR << "SSL handshake failed: " << errBuf;
            conn_m->shutdown();  // 关闭连接
            break;
        }
    }
}
void SslConnection::onEncrypted(const char* data, size_t len) 
{
    writeBuffer_m.append(data, len);
    conn_m->send(&writeBuffer_m);
}

void SslConnection::onDecrypted(const char* data, size_t len) 
{
    decryptedBuffer_m.append(data, len);
}
SSLError SslConnection::getLastError(int ret) 
{
    int err = SSL_get_error(ssl_m, ret);
    switch (err) 
    {
        case SSL_ERROR_NONE:
            return SSLError::NONE;
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            return SSLError::SYSCALL;
        case SSL_ERROR_SSL:
            return SSLError::SSL;
        default:
            return SSLError::UNKNOWN;
    }
}
void SslConnection::handleError(SSLError error) 
{
    switch (error) 
    {
        case SSLError::WANT_READ:
        case SSLError::WANT_WRITE:
            // 需要等待更多数据或写入缓冲区可用
            break;
        case SSLError::SSL:
        case SSLError::SYSCALL:
        case SSLError::UNKNOWN:
            LOG_ERROR << "SSL error occurred: " << ERR_error_string(ERR_get_error(), nullptr);
            state_m = SSLState::ERROR;
            conn_m->shutdown();
            break;
        default:
            break;
    }
}
}