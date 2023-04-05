#pragma once

#include "ikcp.h"
#include "../buffer.h"
#include "../common.h"
#include "../socket_stream.h"

namespace arch_net {

enum SideType {
    Client,
    Server
};

static bool RecvDone = 1;

class UDPSocketStream : public ISocketStream {
public:
    UDPSocketStream(uint32_t conn_id, int fd, EndPoint addr, SideType type);
    ~UDPSocketStream();

public:

    ssize_t recv(Buffer *buff) override;

    ssize_t recv(void *buf, size_t count, int flags) override ;

    ssize_t recv(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t send(Buffer *buff) override;

    ssize_t send(const void *buf, size_t count, int flags) override;

    ssize_t send(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t sendfile(int in_fd, off_t offset, size_t count) override;

    int get_fd() override;

    int close() override { return arch_net::close(sock_fd_); }

    void recv_done(Buffer* buff) {
        recv_buf_.Append(buff->NextAll());
        recv_chn_.push(&RecvDone);
    }

private:
    struct KCPDeleter {
        void operator()(ikcpcb* b) { ikcp_release(b); }
    };

    static int kcp_output(const char *buf, int len, struct IKCPCB *kcp, void *user);

private:
    std::unique_ptr<ikcpcb, KCPDeleter> kcp_;
    int sock_fd_;
    EndPoint peer_addr_;
    SideType type_;
    Buffer recv_buf_;
    acl::fiber_tbox<bool> timer_chn_;
    acl::fiber_tbox<bool> recv_chn_;
};

class UDPSocketClient : public ISocketClient {
public:
    ISocketStream *connect(const std::string &remote, int port) override;

    ISocketStream * connect(EndPoint remote) override;

    ISocketStream *connect(const std::string &path) override;
};


class UDPSocketServer : public ISocketServer {
public:
    int init(const std::string &addr, uint16_t port) override;

    int init(const std::string &path) override;

    virtual ISocketStream *accept() override;

    ISocketServer *set_handler(Handler &&handler) override;

    int start(int thread_num) override;

    void stop() override;

protected:
    void handler(const Handler& m_handler, const std::string& cli_addr, ISocketStream* sess) {
        m_handler(sess);
        streams_.erase(cli_addr);
        delete sess;
    }

    virtual int accept_loop(int index);

private:
    std::vector<int> listen_fds_;
    Handler handler_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::unordered_map<std::string, UDPSocketStream*> streams_;
    std::unordered_map<EndPoint, UDPSocketStream*> ep_streams_;

};

}
