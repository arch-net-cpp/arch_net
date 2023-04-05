#pragma once
#include "socket.h"
#include "common.h"

namespace arch_net {

class Buffer;

struct EndPoint {
    std::string host;
    int port;
    SocketType type;
    sockaddr_storage sock_addr{};

    EndPoint() {}
    // IPV4: 192.168.1.1:8000 IPV6 []:8000
    bool from(const std::string& addr);
    bool from(const std::string& _host, int _port);

    sockaddr* to_sockaddr() {return sockaddr_cast(&sock_addr);}
    friend inline bool operator==(const EndPoint& s1, const EndPoint& s2) {
        return memcmp(&s1.sock_addr, &s2.sock_addr, sizeof(struct sockaddr_storage));
    }
    friend inline bool operator<(const EndPoint& s1, const EndPoint& s2) {
        if (!s1.host.empty() && !s2.host.empty()) {
            return s1.host == s2.host ? s1.port < s2.port : s1.host < s2.host;
        }
        return ToIPPort(&s1.sock_addr) < ToIPPort(&s2.sock_addr);
    }
};

class ISocketStream {
public:
    // recv some bytes from the socket;
    virtual ssize_t recv(void *buf, size_t count, int flags = 0) = 0;
    virtual ssize_t recv(const struct iovec *iov, int iovcnt, int flags = 0) = 0;
    virtual ssize_t recv(Buffer *buff) = 0;

    // send some bytes to the socket;
    virtual ssize_t send(const void *buf, size_t count, int flags = 0) = 0;
    virtual ssize_t send(const struct iovec *iov, int iovcnt, int flags = 0) = 0;
    virtual ssize_t send(Buffer *buff) = 0;
    virtual ssize_t sendfile(int in_fd, off_t offset, size_t count) = 0;

    virtual int close() { return 0; }

    virtual int get_fd() = 0;

    virtual ~ISocketStream(){}
};

class ISocketClient {
public:
    // Connect to a remote IPv4 endpoint.
    virtual ISocketStream* connect(const std::string& remote, int port) = 0;
    // Connect to universal remote endpoint.
    virtual ISocketStream* connect(EndPoint remote) = 0;
    // Connect to a Unix Domain Socket.
    virtual ISocketStream* connect(const std::string& path) = 0;

    virtual ~ISocketClient(){};
};

class ISocketServer  {
public:
    virtual int init(const std::string& addr, uint16_t port) = 0;
    virtual int init(const std::string& path) = 0;
    virtual ISocketStream* accept() = 0;

    using Handler = std::function<int(ISocketStream*)>;
    virtual ISocketServer* set_handler(Handler&& handler) = 0;
    virtual int start(int io_thread_num = 1) = 0;
    virtual void stop() = 0;

    virtual int get_listen_fd() { return -1; }

    virtual ~ISocketServer(){};
};

class TcpSocketStream : public ISocketStream {
public:
    explicit TcpSocketStream(int fd) : fd_(fd) {}
    explicit TcpSocketStream(const SocketType& type) {
        switch (type.type) {
            case SocketType::IPV4:
                fd_ = arch_net::socket();
                break;
            case SocketType::IPV6:
                fd_ = arch_net::socket6();
                break;
            case SocketType::UNIX:
                fd_ = arch_net::unix_socket();
                break;
            default:
                fd_ = arch_net::socket();
        }
    }
    virtual ~TcpSocketStream() {
        if (fd_ < 0) return;
        arch_net::close(fd_);
    }

    ssize_t recv(Buffer *buff) override;

    ssize_t recv(void *buf, size_t count, int flags) override;

    ssize_t recv(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t send(Buffer *buff) override;

    ssize_t send(const void *buf, size_t count, int flags) override;

    ssize_t send(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t sendfile(int in_fd, off_t offset, size_t count) override;

    int close() override {
        if (fd_ < 0) return 0;
        return arch_net::close(fd_);
    }

    int get_fd() override { return fd_; }
private:
    int fd_ = -1;
};


class TcpSocketClient : public ISocketClient {

public:
    TcpSocketClient() {};

    ISocketStream *connect(const std::string &remote, int port) override;

    ISocketStream *connect(const std::string &path) override;

    ISocketStream * connect(EndPoint remote) override;

    virtual TcpSocketStream* create_stream(SocketType type);
};


class TcpSocketServer : public ISocketServer {
public:
    int init(const std::string &addr, uint16_t port) override;

    int init(const std::string &path) override;

    virtual ISocketStream *accept() override;

    ISocketServer *set_handler(Handler&& handler) override;

    int start(int thread_num) override;

    void stop() override;

    int get_listen_fd() override { return listen_fd_; }

    virtual int accept_loop();


protected:

    TcpSocketStream* create_stream(int fd) {
        return new TcpSocketStream(fd);
    }

    static void handler(const Handler& m_handler, ISocketStream* sess) {
        m_handler(sess);
        delete sess;
    }

private:
    int listen_fd_;
    Handler handler_;

    std::vector<std::unique_ptr<std::thread>> threads_;
    acl::wait_group wg_;
};

typedef ISocketStream* SocketStreamPtr;

extern "C" ISocketClient* new_tcp_socket_client();
extern "C" ISocketServer* new_tcp_socket_server();

}

// for EndPoint
namespace std {
template <>
struct hash<arch_net::EndPoint> {
    std::size_t operator()(const arch_net::EndPoint& end_point) const {
        return std::hash<std::string>()(std::string((const char*)&(end_point.sock_addr), sizeof(struct sockaddr_storage)));
    }
};

}  // namespace std