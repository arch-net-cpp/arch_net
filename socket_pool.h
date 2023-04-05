#pragma once
#include "common.h"
#include "socket_stream.h"

namespace arch_net {

class TcpSocketPoolClient;

class PooledTCPSocketStream : public ISocketStream {
public:
    PooledTCPSocketStream(ISocketStream* stream, TcpSocketPoolClient* pool, const EndPoint& ep)
        : underlay(stream), pool(pool), end_point(ep), drop(false) {
        int optval = 1;
        ::setsockopt(underlay->get_fd(), SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    }

    ~PooledTCPSocketStream();

    ssize_t recv(void *buf, size_t count, int flags) override {
        auto ret = underlay->recv(buf, count, flags);
        if (ret <=0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t recv(const struct iovec *iov, int iovcnt, int flags) override {
        auto ret = underlay->recv(iov, iovcnt, flags);
        if (ret <= 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t recv(Buffer *buff) override {
        auto ret = underlay->recv(buff);
        if (ret <= 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t send(Buffer *buff) override {
        auto ret = underlay->send(buff);
        if (ret < 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t send(const void *buf, size_t count, int flags) override {
        auto ret = underlay->send(buf, count, flags);
        if (ret < 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t send(const struct iovec *iov, int iovcnt, int flags) override {
        auto ret = underlay->send(iov, iovcnt, flags);
        if (ret < 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    ssize_t sendfile(int in_fd, off_t offset, size_t count) override {
        auto ret = underlay->sendfile(in_fd, offset, count);
        if (ret < 0 ) {
            drop = true;
            return -1;
        }
        return ret;
    }

    int get_fd() override {
        return underlay->get_fd();
    }

    int close() override {
        return underlay->close();
    }

public:
    ISocketStream* underlay;
    TcpSocketPoolClient* pool;
    EndPoint end_point;
    bool drop;
};

struct StreamListNode : public intrusive_list_node<StreamListNode> {
    EndPoint key;
    std::unique_ptr<ISocketStream> stream;

    StreamListNode() {}
    StreamListNode(const EndPoint& key, ISocketStream* stream)
        : key(key), stream(stream){}
};


class TcpSocketPoolClient : public ISocketClient {
public:
    TcpSocketPoolClient(ISocketClient* client, bool client_ownership)
        : client_(client), client_ownership_(client_ownership), exit_chn_(){

        collector_ = create_fiber([&]()->int {
            std::vector<int> fds, readable_fds;
            while (true) {
                fds.clear();
                readable_fds.clear();

                bool found = false;
                exit_chn_.pop(10000, &found);
                if (found) break;
                mutex_.lock();
                for(auto & pair : fd2ep_map_) {
                    fds.emplace_back(pair.first);
                }
                mutex_.unlock();
                if (wait_fds_readable(fds, readable_fds, 0) <= 0) {
                    continue;
                }
                mutex_.lock();
                for (auto& fd : readable_fds) {
                    drop_from_pool(fd);
                }
                mutex_.unlock();
            }
            wait_chn_.push(nullptr);
            return -1;
        });
    }

    ~TcpSocketPoolClient() {
        exit_chn_.push(nullptr);
        (void)collector_;
        wait_chn_.pop();
        while (!stream_list_.empty()) {
            auto node = stream_list_.pop_front();
            delete node;
        }
        if (client_ownership_) {
            delete client_;
        }
    }

    ISocketStream *connect(const std::string &remote, int port) override {
        EndPoint ep;
        ep.from(remote, port);
        return connect(ep);
    }

    ISocketStream *connect(const std::string &path) override {
        throw "not implementation";
    }

    ISocketStream * connect(EndPoint remote) override {
        mutex_.lock();
        while (true) {
            auto it = ep2stream_map_.find(remote);
            if (it == ep2stream_map_.end()) {
                mutex_.unlock();

                auto stream = client_->connect(remote);
                if (stream) {
                    return new PooledTCPSocketStream(stream, this, remote);
                }
                return nullptr;
            } else {
                auto fd = it->second->stream->get_fd();
                if (fd >= 0) {
                    fd2ep_map_.erase(fd);
                }
                auto node = it->second;
                ep2stream_map_.erase(it);
                stream_list_.erase(node);
                if (fd >= 0 && wait_fd_read_timeout(fd, 0) == 1) {
                    delete node;
                    continue;
                }
                auto ret = new PooledTCPSocketStream(dynamic_cast<TcpSocketStream*>(node->stream.release()), this, remote);
                delete node;
                mutex_.unlock();
                return ret;
            }
        }
    }

    bool release(const EndPoint& ep, ISocketStream* stream) {
        mutex_.lock();
        defer(mutex_.unlock());
        auto fd = stream->get_fd();
        if (fd >= 0) {
            if (wait_fd_read_timeout(fd, 0) == 1) {
                return false;
            }
            fd2ep_map_.emplace(fd, ep);
        }
        // stream back to pool
        auto node = new StreamListNode(ep, stream);
        ep2stream_map_.emplace(ep, node);
        stream_list_.push_back(node);
        return true;
    }

    void drop_from_pool(int fd) {
        // find fdep & fdmap and remove
        auto it = fd2ep_map_.find(fd);
        if (it != fd2ep_map_.end()) {
            auto ep = it->second;
            for (auto map_it = ep2stream_map_.find(ep); map_it != ep2stream_map_.end() && map_it->first == ep; map_it++) {
                if (map_it->second->stream->get_fd() == fd) {
                    auto node = map_it->second;
                    ep2stream_map_.erase(map_it);
                    stream_list_.erase(node);
                    delete node;
                    break;
                }
            }
            fd2ep_map_.erase(it);
        }
    }

private:
    ISocketClient* client_;
    bool client_ownership_;
    ACL_FIBER* collector_;
    acl::fiber_mutex mutex_;
    acl::fiber_tbox<bool> exit_chn_;
    acl::fiber_tbox<bool> wait_chn_;
    std::unordered_multimap<EndPoint, StreamListNode*> ep2stream_map_;
    std::unordered_map<int, EndPoint> fd2ep_map_;
    intrusive_list<StreamListNode> stream_list_;
};

}