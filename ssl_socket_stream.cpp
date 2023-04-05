#include "ssl_socket_stream.h"

namespace arch_net { namespace ssl {

int TLSContext::init(const std::string &ca_pem,
                     const std::string &cert_pem,
                     const std::string &key_pem) {
    switch (type_) {
        case EncryptType::CLIENT:
            ctx_ = get_client_context(ca_pem.c_str(), cert_pem.c_str(), key_pem.c_str());
            if (ctx_ == nullptr) {
                return ERR;
            }
            return OK;
        case EncryptType::SERVER:
            ctx_ = get_server_context(ca_pem.c_str(), cert_pem.c_str(), key_pem.c_str());
            if (ctx_ == nullptr) {
                return ERR;
            }
            return OK;
    }
    return ERR;
}


TLSSocketStream::TLSSocketStream(TLSContext *ctx, ISocketStream *stream, bool ownership)
    : inner_stream_(stream), m_ownership_(ownership) {

    ssl = SSL_new(ctx->ssl_ctx());
    SSL_set_fd(ssl, stream->get_fd());

    ssbio = BIO_new(BIO_s_sock_stream());

    BIO_ctrl(ssbio, BIO_C_SET_FILE_PTR, 0, stream);
    SSL_set_bio(ssl, ssbio, ssbio);

    switch (ctx->encrypt_type()) {
        case EncryptType::CLIENT:
            SSL_set_connect_state(ssl);
            break;
        case EncryptType::SERVER:
            SSL_set_accept_state(ssl);
            break;
        default:
            break;
    }
}

TLSSocketStream::~TLSSocketStream() {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    if (m_ownership_) {
        delete inner_stream_;
    }
}

int TLSSocketStream::client_handshake() {
    /* Perform SSL handshake with the server */
    if (SSL_do_handshake(ssl) != 1) {
        LOG(ERROR) << "SSL Handshake failed";
        return ERR;
    }

    /* Verify that SSL handshake completed successfully */
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        LOG(ERROR) << "Verification of handshake failed";
        return ERR;
    }
    return OK;
}

int TLSSocketStream::server_handshake() {
    /* Now perform handshake */
    int rc;
    if ((rc = SSL_accept(ssl)) != 1) {
        LOG(ERROR) << "Could not perform SSL handshake, with return value: " << rc;
        return ERR;
    }
    return OK;
}

BIO_METHOD *TLSSocketStream::BIO_s_sock_stream() {
    static std::unique_ptr<BIO_METHOD, BIOMethodDeleter> meth(
            BIO_meth_new(BIO_TYPE_SOURCE_SINK, "BIO_ARCH_NET_SOCK_STREAM"));
    auto ret = meth.get();
    BIO_meth_set_write(ret, &TLSSocketStream::ssbio_bwrite);
    BIO_meth_set_read(ret, &TLSSocketStream::ssbio_bread);
    BIO_meth_set_puts(ret, &TLSSocketStream::ssbio_bputs);
    BIO_meth_set_ctrl(ret, &TLSSocketStream::ssbio_ctrl);
    BIO_meth_set_create(ret, &TLSSocketStream::ssbio_create);
    BIO_meth_set_destroy(ret, &TLSSocketStream::ssbio_destroy);
    return ret;
}


ISocketStream *TLSSocketClient::connect(const std::string &path) {
    // connect to server
    auto stream = new_tls_stream(ctx, inner_client_->connect(path), true);
    if (!stream) {
        return nullptr;
    }

    std::unique_ptr<ISocketStream> stream_scope(stream);
    auto tls_stream = dynamic_cast<TLSSocketStream*>(stream_scope.get());
    if (!tls_stream) {
        return nullptr;
    }

    int ret = tls_stream->client_handshake();
    if (ret < 0) {
        return nullptr;
    }

    return stream_scope.release();
}

ISocketStream *TLSSocketClient::connect(const std::string &remote, int port) {
    // connect to server
    EndPoint ep;
    ep.from(remote, port);
    return connect(ep);
}

ISocketStream *TLSSocketClient::connect(EndPoint remote) {
    auto stream = new_tls_stream(ctx, inner_client_->connect(remote), true);
    if (!stream) {
        return nullptr;
    }

    std::unique_ptr<ISocketStream> stream_scope(stream);
    auto tls_stream = dynamic_cast<TLSSocketStream*>(stream_scope.get());

    int ret = tls_stream->client_handshake();
    if (ret < 0) {
        return nullptr;
    }

    return stream_scope.release();
}

ISocketStream *TLSSocketServer::accept() {
    int cfd = arch_net::accept(get_listen_fd());
    if (cfd < 0) {
        return nullptr;
    }

    auto stream = new_tls_stream(ctx, create_stream(cfd), true);
    if (!stream) {
        return nullptr;
    }

    std::unique_ptr<ISocketStream> stream_scope(stream);
    auto tls_stream = dynamic_cast<TLSSocketStream*>(stream_scope.get());

    int ret = tls_stream->server_handshake();
    if (ret < 0) {
        return nullptr;
    }

    return stream_scope.release();
}


void OpenSSLGlobalInit() { (void)GlobalSSLContext::getInstance(); }

TLSContext* new_server_tls_context(const std::string& ca_pem,
                                   const std::string& cert_pem,
                                   const std::string& key_pem)
                                   {
    OpenSSLGlobalInit();
    std::unique_ptr<TLSContext> ctx = std::make_unique<TLSContext>(EncryptType::SERVER);
    if (ctx->init(ca_pem, cert_pem, key_pem) != OK)
        return nullptr;
    return ctx.release();
}

TLSContext* new_client_tls_context(const std::string& ca_pem,
                                   const std::string& cert_pem,
                                   const std::string& key_pem) {
    OpenSSLGlobalInit();
    std::unique_ptr<TLSContext> ctx = std::make_unique<TLSContext>(EncryptType::CLIENT);
    if (ctx->init(ca_pem, cert_pem, key_pem) != OK) {
        return nullptr;
    }
    return ctx.release();
}

ISocketStream* new_tls_stream(TLSContext* ctx, ISocketStream* base, bool ownership) {
    if (!ctx || !base) {
        LOG(ERROR) << "invalid parameters";
        return nullptr;
    }
    return new TLSSocketStream(ctx, base, ownership);
}

ISocketClient* new_tls_client(TLSContext* ctx, ISocketClient* base,
                              bool ownership) {
    if (!ctx || !base) {
        LOG(ERROR) << "invalid parameters";
        return nullptr;
    }
    return new TLSSocketClient(ctx, base, ownership);
}

ISocketServer* new_tls_server(TLSContext* ctx) {
    if (!ctx) {
        LOG(ERROR) << "invalid parameters";
        return nullptr;
    }
    return new TLSSocketServer(ctx);
}


}
}