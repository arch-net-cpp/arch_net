
#include "socket_stream.h"
#include "buffer.h"
namespace arch_net {

bool EndPoint::from(const std::string &addr) {
    if (!SplitHostPort(addr, host, port)) {
        return false;
    }
    bool is_v6;
    if (!ParseFromIPPort(host, port, sock_addr, is_v6)) {
        return false;
    }
    if (is_v6) {
        type = SocketType(SocketType::IPV6);
    }
    return true;
}

bool EndPoint::from(const std::string &_host, int _port) {
    host.assign(_host);
    port = _port;
    bool is_v6;
    if (!ParseFromIPPort(host, port, sock_addr, is_v6)) {
        return false;
    }
    if (is_v6) {
        type = SocketType(SocketType::IPV6);
    }
    return true;
}

ssize_t TcpSocketStream::recv(Buffer *buff) {
    auto n = recv(buff->WriteBegin(), buff->WritableBytes(), 0);
    if (n > 0) {
        buff->WriteBytes(n);
    }
    return n;
}

ssize_t TcpSocketStream::send(Buffer *buff) {
    auto n = send(buff->data(), buff->size(), 0);
    if (n > 0) {
        buff->Retrieve(n);
    }
    return n;
}

ssize_t TcpSocketStream::recv(void *buf, size_t count, int flags) {
    return arch_net::read(fd_, buf, count);
}

ssize_t TcpSocketStream::recv(const struct iovec *iov, int iovcnt, int flags) {
    return arch_net::readv(fd_, iov, iovcnt);
}

ssize_t TcpSocketStream::send(const void *buf, size_t count, int flags) {
    return arch_net::write(fd_, buf, count);
}

ssize_t TcpSocketStream::send(const struct iovec *iov, int iovcnt, int flags) {
    return arch_net::writev(fd_, iov, iovcnt);
}

ssize_t TcpSocketStream::sendfile(int in_fd, off_t offset, size_t count) {
    return 0;
}


TcpSocketStream *TcpSocketClient::create_stream(SocketType type) {
    return new TcpSocketStream(type);
}

ISocketStream *TcpSocketClient::connect(const std::string &path) {
    auto stream = create_stream(SocketType(SocketType::UNIX));
    std::unique_ptr<TcpSocketStream> stream_scope(stream);
    if (!stream_scope || stream_scope->get_fd() < 0) {
        LOG(ERROR) << "Failed to create socket fd";
        return nullptr;
    }

    auto ret = arch_net::connect(stream_scope->get_fd(), path);
    if (ret < 0) {
        LOG(ERROR) << "Failed to connect socket";
        return nullptr;
    }
    return stream_scope.release();
}

ISocketStream *TcpSocketClient::connect(const std::string &remote, int port) {
    EndPoint ep;
    if (ep.from(remote, port)) {
        return connect(ep);
    }
    return nullptr;
}

ISocketStream *TcpSocketClient::connect(EndPoint remote) {
    auto stream = create_stream(SocketType(remote.type));
    std::unique_ptr<TcpSocketStream> stream_scope(stream);
    if (!stream_scope || stream_scope->get_fd() < 0) {
        LOG(ERROR) << "Failed to create socket fd";
        return nullptr;
    }

    auto ret = arch_net::connect(stream_scope->get_fd(), remote.sock_addr);
    if (ret < 0) {
        LOG(ERROR) << "Failed to connect socket";
        return nullptr;
    }
    return stream_scope.release();
}

// TcpSocketServer impl
int TcpSocketServer::init(const std::string &addr, uint16_t port) {
    listen_fd_ = arch_net::socket();
    return arch_net::listen(listen_fd_, addr.c_str(), port);
}

int TcpSocketServer::init(const std::string &path) {
    listen_fd_ = arch_net::unix_socket();
    return arch_net::listen(listen_fd_, path);
}

ISocketStream *TcpSocketServer::accept() {
    int cfd = arch_net::accept(listen_fd_);
    if (cfd < 0) {
        return nullptr;
    }
    return create_stream(cfd);
}

ISocketServer *TcpSocketServer::set_handler(ISocketServer::Handler&& handler) {
    handler_ = std::move(handler);
    return this;
}

int TcpSocketServer::accept_loop() {
    while (true) {
        auto sess = accept();
        if (sess == nullptr) {
            LOG(ERROR) << "accept new connection error";
            acl_fiber_delay(1);
            break;
        }
        // create new fiber
        go[this, sess] {
            arch_net::TcpSocketServer::handler(this->handler_, sess);
        };
    }
    return 0;
}

int TcpSocketServer::start(int io_thread_num) {

    for (int i = 0; i < io_thread_num; i++) {
        auto thread = std::make_unique<std::thread>(
            [this](){
                go[this] {
                    this->accept_loop();
                };
                acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
                wg_.done();
            });
        threads_.emplace_back(std::move(thread));
    }

    for (int i = 0; i < io_thread_num; i++) {
        threads_[i]->join();
    }
    return 0;
}

void TcpSocketServer::stop() {

}



extern "C" ISocketClient* new_tcp_socket_client() {
    return new TcpSocketClient();
}
extern "C" ISocketServer* new_tcp_socket_server() {
    return new TcpSocketServer();
}

}