#include "SslContext.h"
#include <muduo/base/Logging.h>
#include <openssl/err.h>


namespace ssl{
ssl::SslContext::SslContext(const SslConfig &config)
    :ctx_m(nullptr),config_m(config)
{

}

SslContext::~SslContext()
{
    if(ctx_m){
        SSL_CTX_free(ctx_m);
    }
}

bool ssl::SslContext::initialize()
{
    // 初始化 OpenSSL
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | 
                    OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);

    // 创建 SSL 上下文
    const SSL_METHOD* method = TLS_server_method();
    ctx_m = SSL_CTX_new(method);
    if (!ctx_m)
    {
        handleSslError("Failed to create SSL context");
        return false;
    }

    // 设置 SSL 选项
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | 
                  SSL_OP_NO_COMPRESSION |
                  SSL_OP_CIPHER_SERVER_PREFERENCE;
    SSL_CTX_set_options(ctx_m, options);

    // 加载证书和私钥
    if (!loadCertificates())
    {
        return false;
    }

    // 设置协议版本
    if (!setupProtocol())
    {
        return false;
    }

    // 设置会话缓存
    setupSessionCache();

    LOG_INFO << "SSL context initialized successfully";
    return true;
}

bool ssl::SslContext::loadCertificates()
{
    // 加载证书
    if (SSL_CTX_use_certificate_file(ctx_m,
     config_m.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load server certificate");
        return false;
    }

    // 加载私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_m, 
        config_m.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load private key");
        return false;
    }

    // 验证私钥
    if (!SSL_CTX_check_private_key(ctx_m))
    {
        handleSslError("Private key does not match the certificate");
        return false;
    }

    // 加载证书链
    if (!config_m.getCertificateChainFile().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(ctx_m,
            config_m.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("Failed to load certificate chain");
            return false;
        }
    }

    return true;
}

bool ssl::SslContext::setupProtocol()
{
    // 设置 SSL/TLS 协议版本
    SSL_CTX_set_options(ctx_m,SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

    // 设置协议版本范围
    switch (config_m.getProtocolVersion()) {
        case SSLVersion::TLS_1_0:
            SSL_CTX_set_min_proto_version(ctx_m, TLS1_VERSION);
            SSL_CTX_set_max_proto_version(ctx_m, TLS1_VERSION);
            break;
        case SSLVersion::TLS_1_1:
            SSL_CTX_set_min_proto_version(ctx_m, TLS1_1_VERSION);
            SSL_CTX_set_max_proto_version(ctx_m, TLS1_1_VERSION);
            break;
        case SSLVersion::TLS_1_2:
            SSL_CTX_set_min_proto_version(ctx_m, TLS1_2_VERSION);
            break; // 允许 TLS 1.2+
        case SSLVersion::TLS_1_3:
            SSL_CTX_set_min_proto_version(ctx_m, TLS1_3_VERSION);
            break;
    }
    
    
    // 设置加密套件
    if (!config_m.getCipherList().empty())
    {
        if (SSL_CTX_set_cipher_list(ctx_m,
            config_m.getCipherList().c_str()) <= 0)
        {
            handleSslError("Failed to set cipher list");
            return false;
        }
    }

    return true;
}

void ssl::SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(ctx_m, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(ctx_m, config_m.getSessionCacheSize());
    SSL_CTX_set_timeout(ctx_m, config_m.getSessionTimeout());
}

void ssl::SslContext::handleSslError(const char *msg)
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    LOG_ERROR << msg << ": " << buf;
}
}
