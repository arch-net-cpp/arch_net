#pragma once
#include "common.h"
#include "socket_stream.h"
#include "buffer.h"

namespace arch_net { namespace ssl {

enum EncryptType {
    CLIENT,
    SERVER
};

class GlobalSSLContext : public Singleton<GlobalSSLContext> {
public:
    GlobalSSLContext() {
        SSL_load_error_strings();
        SSL_library_init();
        OpenSSL_add_all_ciphers();
        OpenSSL_add_all_digests();
        OpenSSL_add_all_algorithms();
    }
    ~GlobalSSLContext() {}
};

class TLSContext {
public:
    TLSContext(EncryptType type) :type_(type) {}
    int init(const std::string& ca_pem,
             const std::string& cert_pem,
             const std::string& key_pem);

    SSL_CTX* ssl_ctx() { return ctx_; }

    EncryptType encrypt_type() { return type_; }

private:
    SSL_CTX* ctx_;
    EncryptType type_;
};


TLSContext* new_server_tls_context(const std::string& ca_pem,
                            const std::string& cert_pem,
                            const std::string& key_pem);

TLSContext* new_client_tls_context(const std::string& ca_pem,
                                   const std::string& cert_pem,
                                   const std::string& key_pem) ;


class TLSSocketStream : public ISocketStream {
public:
    TLSSocketStream(TLSContext* ctx, ISocketStream* stream, bool ownership = false);

    ~TLSSocketStream();

    int client_handshake();

    int server_handshake();

    ssize_t recv(Buffer *buff) override {
        auto n = recv(buff->WriteBegin(), buff->WritableBytes(), 0);
        if (n > 0) {
            buff->WriteBytes(n);
        }
        return n; }

    ssize_t recv(void* buf, size_t cnt, int flags = 0) override { return SSL_read(ssl, buf, cnt);}

    ssize_t recv(const struct iovec* iov, int iovcnt, int flags = 0) override { return recv(iov[0].iov_base, iov[0].iov_len); }

    ssize_t send(Buffer *buff) override {
        auto n = send(buff->data(), buff->size(), 0);
        if (n > 0) {
            buff->Retrieve(n);
        }
        return n;
    }

    ssize_t send(const void* buf, size_t cnt, int flags = 0) override { return SSL_write(ssl, buf, cnt);}

    ssize_t send(const struct iovec* iov, int iovcnt, int flags = 0) override { return send(iov[0].iov_base, iov[0].iov_len); }

    ssize_t sendfile(int fd, off_t offset, size_t size) override {return -2;} // Not support

    int get_fd() override { return inner_stream_->get_fd();}

    int close() override { return inner_stream_->close(); }

private:
    struct BIOMethodDeleter {
        void operator()(BIO_METHOD* ptr) { BIO_meth_free(ptr); }
    };

    static BIO_METHOD* BIO_s_sock_stream();

    static ISocketStream* get_bio_sock_stream(BIO* b) { return (ISocketStream*)BIO_get_data(b);}

    static int ssbio_bwrite(BIO* b, const char* buf, int cnt) { return get_bio_sock_stream(b)->send(buf, cnt);}

    static int ssbio_bread(BIO* b, char* buf, int cnt) { return get_bio_sock_stream(b)->recv(buf, cnt);}

    static int ssbio_bputs(BIO* bio, const char* str) { return ssbio_bwrite(bio, str, strlen(str));}

    static long ssbio_ctrl(BIO* b, int cmd, long num, void* ptr) {
        long ret = 1;

        switch (cmd) {
            case BIO_C_SET_FILE_PTR:
                BIO_set_data(b, ptr);
                BIO_set_shutdown(b, num);
                BIO_set_init(b, 1);
                break;
            case BIO_CTRL_GET_CLOSE:
                ret = BIO_get_shutdown(b);
                break;
            case BIO_CTRL_SET_CLOSE:
                BIO_set_shutdown(b, num);
                break;
            case BIO_CTRL_DUP:
            case BIO_CTRL_FLUSH:
                ret = 1;
                break;
            default:
                ret = 0;
                break;
        }
        return (ret);
    }

    static int ssbio_create(BIO* bi) {
        BIO_set_data(bi, nullptr);
        BIO_set_init(bi, 0);
        return (1);
    }

    static int ssbio_destroy(BIO*) { return 1; }

private:
    ISocketStream* inner_stream_;
    bool m_ownership_;
    SSL* ssl;
    BIO* ssbio;
};


ISocketStream* new_tls_stream(TLSContext* ctx, ISocketStream* base, bool ownership);


class TLSSocketClient : public ISocketClient{
public:
    TLSContext* ctx;
    ISocketClient* inner_client_;
    bool ownership_;

    TLSSocketClient(TLSContext* ctx, ISocketClient* underlay, bool ownership)
        : ctx(ctx), inner_client_(underlay), ownership_(ownership) {}

    ~TLSSocketClient() {
        if (ownership_) delete inner_client_;
    }

    virtual ISocketStream* connect(const std::string& path) override;

    virtual ISocketStream* connect(EndPoint remote) override;

    virtual ISocketStream* connect(const std::string& remote, int port) override;

};


class TLSSocketServer : public TcpSocketServer {
public:

    TLSSocketServer(TLSContext* ctx): ctx(ctx) {}

    ISocketStream *accept() override;

private:public:
    TLSContext* ctx;
};


ISocketClient* new_tls_client(TLSContext* ctx, ISocketClient* base, bool ownership);

ISocketServer* new_tls_server(TLSContext* ctx);

}}


