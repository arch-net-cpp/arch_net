
#include "udp_socket_stream.h"

namespace arch_net {

UDPSocketStream::UDPSocketStream(uint32_t conn_id, int fd, EndPoint addr, SideType type)
        : kcp_(ikcp_create(conn_id, this)), sock_fd_(fd), peer_addr_(addr), type_(type),
          recv_buf_(2048, 0), timer_chn_(), recv_chn_(false) {
#if defined(DISABLE_KCP)

#else
    kcp_->output = UDPSocketStream::kcp_output;
    ikcp_update(kcp_.get(), iclock());
    go [&] {
        auto current = iclock();
        while (true) {
            auto next_time = ikcp_check(kcp_.get(), current);
            bool found;
            timer_chn_.pop(next_time - current, &found);
            if (found) {
                return;
            }
            current = iclock();
            ikcp_update(kcp_.get(), current);
        }
    };
#endif
}

UDPSocketStream::~UDPSocketStream() {
    timer_chn_.push(nullptr);
}

ssize_t UDPSocketStream::recv(Buffer *buff) {
    auto n = recv(buff->WriteBegin(), buff->WritableBytes(), 0);
    if (n > 0) {
        buff->WriteBytes(n);
    }
    return n;
}

ssize_t UDPSocketStream::recv(void *buf, size_t count, int flags) {
#if defined(DISABLE_KCP)
    if (type_ == SideType::Client) {
        return arch_net::recvfrom(sock_fd_, recv_buf_.WriteBegin(), recv_buf_.WritableBytes(), flags, (struct sockaddr*)&peer_addr_, &rsz);
    } else {

    }
#else
    if (type_ == SideType::Client) {
        while (true) {
            int nrecv = ikcp_recv(kcp_.get(), static_cast<char *>(buf), count);
            // get one packet
            if (nrecv >= 0 ) return nrecv;

            recv_buf_.Reset();
            socklen_t addr_len = sizeof(struct sockaddr);
            auto n = arch_net::recvfrom(sock_fd_, recv_buf_.WriteBegin(), recv_buf_.WritableBytes(),flags,
                                        peer_addr_.to_sockaddr(), &addr_len);
            if (n < 0) {
                return -1;
            }
            recv_buf_.WriteBytes(n);
            ikcp_input(kcp_.get(), recv_buf_.data(), recv_buf_.size());
            //ikcp_update(kcp_.get(), iclock());
        }
    } else {
        while (true) {
            int nrecv = ikcp_recv(kcp_.get(), static_cast<char *>(buf), count);
            // get one packet
            if (nrecv >=0 ) return nrecv;
            // wait signal
            auto res = recv_chn_.pop();
            if (!res) {
                return 0;
            }

            ikcp_input(kcp_.get(), recv_buf_.data(), recv_buf_.size());
            //ikcp_update(kcp_.get(), iclock());
            recv_buf_.Reset();
        }
    }
#endif
}


ssize_t UDPSocketStream::recv(const struct iovec *iov, int iovcnt, int flags) {
    return 0;
}

ssize_t UDPSocketStream::send(Buffer *buff) {
    auto n = send(buff->data(), buff->size(), 0);
    if (n > 0) {
        buff->Retrieve(n);
    }
    return n;
}

ssize_t UDPSocketStream::send(const void *buf, size_t count, int flags) {
    int ret =  ikcp_send(kcp_.get(), static_cast<const char *>(buf), count);
    if (ret < 0) {
        return -1;
    }
    ikcp_update(kcp_.get(), iclock());
    return count;
}

ssize_t UDPSocketStream::send(const struct iovec *iov, int iovcnt, int flags) {
    return 0;
}

ssize_t UDPSocketStream::sendfile(int in_fd, off_t offset, size_t count) {
    return 0;
}

int UDPSocketStream::get_fd() {
    return 0;
}

int UDPSocketStream::kcp_output(const char *buf, int len, struct IKCPCB *kcp, void *user) {
    auto* stream = (UDPSocketStream*)user;
    socklen_t addr_len = sizeof(struct sockaddr);
    auto n = arch_net::sendto(stream->sock_fd_, buf, len, 0, (stream->peer_addr_.to_sockaddr()),addr_len);
    if (n < 0) {
        LOG(ERROR) << "send error";
        return -1;
    }
    //LOG(INFO) << "kcp out put";
    //ikcp_update(stream->kcp_.get(), iclock());
    return n;
}

ISocketStream *UDPSocketClient::connect(const std::string &remote, int port) {
    EndPoint ep;
    ep.from(remote, port);
    return connect(ep);
}

ISocketStream *UDPSocketClient::connect(const std::string &path) {
    return nullptr;
}

ISocketStream *UDPSocketClient::connect(EndPoint remote) {
    int cli_fd;
    struct sockaddr server;
    switch (remote.type.type) {
    case SocketType::IPV6:
        cli_fd = udp6_socket();
    default:
        cli_fd = udp_socket();
    }

    uint32_t conn_id = time(nullptr);
    Buffer buf(4, 0);
    buf.AppendUInt32(conn_id);

    auto r = arch_net::sendto(cli_fd, buf.data(), buf.size(), 0,
                              remote.to_sockaddr(), sizeof(server));
    if (r < 0) {
        LOG(ERROR) << "create udp stream error";
        return nullptr;
    }
    auto stream_scope = std::make_unique<UDPSocketStream>(conn_id, cli_fd, remote, SideType::Client);

    return stream_scope.release();
}


int UDPSocketServer::init(const std::string &addr, uint16_t port) {
    int fd = udp_server(addr.c_str(), port);
    listen_fds_.push_back(fd);
    return 0;
}

int UDPSocketServer::init(const std::string &path) {
    return 0;
}

ISocketStream *UDPSocketServer::accept() {
    return nullptr;
}

ISocketServer *UDPSocketServer::set_handler(ISocketServer::Handler &&handler) {
    handler_ = handler;
    return this;
}

int UDPSocketServer::accept_loop(int index) {
    Buffer recv_buf(2048, 0);
    EndPoint client;
    socklen_t addr_len = sizeof(struct sockaddr);
    int listen_fd = listen_fds_[index];
    while (true) {
        auto ret = arch_net::recvfrom(listen_fd, recv_buf.WriteBegin(),
                                      recv_buf.WritableBytes(), 0, client.to_sockaddr(), &addr_len);
        if(ret < 0 ) {
            LOG(ERROR) << "recv error";
            acl_fiber_delay(1);
            continue;
        }
        recv_buf.WriteBytes(ret);

        uint32_t conn_id = recv_buf.PeekUInt32();
        std::string client_addr = ToIPPort(&client.sock_addr);
        auto it = streams_.find(client_addr);
        if (it == streams_.end()) {
            auto stream = new UDPSocketStream(conn_id, listen_fd, client, SideType::Server);
            streams_.emplace(client_addr, stream);
            go[&] {
                this->handler(this->handler_, client_addr, stream);
            };
        } else {
            it->second->recv_done(&recv_buf);
        }
        recv_buf.Reset();
    }
}

int UDPSocketServer::start(int thread_num) {

    for (int i = 0; i < listen_fds_.size(); i++) {
        auto thread = std::make_unique<std::thread>(
            [=](){
                go[&] {
                    this->accept_loop(i);
                };
                acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
            });
        threads_.emplace_back(std::move(thread));
    }

    for (int i = 0; i < listen_fds_.size(); i++) {
        threads_[i]->join();
    }
    return 0;
}

void UDPSocketServer::stop() {

}

}