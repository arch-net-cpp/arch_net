#pragma once
#include "common.h"
#include "socket_stream.h"
#include "ssl_socket_stream.h"
#include "mux_stream.h"
#include "buffer.h"
#include "mux_define.h"

namespace arch_net  { namespace mux {

class MultiplexingSession {
public:
    MultiplexingSession(ISocketStream* underlying, bool ownership, MuxStreamType type);

    ~MultiplexingSession();
    // outer interface
    ISocketStream* open_stream();

    ISocketStream* accept_stream();

    void close();

private:
    void session_setup();

    void add_send_task(std::function<int()> func);

    void exit_err() { close(); }

    bool is_closed() { return shut_down_; }

    void close_stream(uint32_t id);

    int recv_loop();

    int handle_stream_message(Header& hdr);

    int incoming_stream(uint32_t id);

    void go_away(Header& hdr, Buffer& buf, uint32_t reason) {
        local_goaway_ = 1;
        hdr.encode(&buf, typeGoAway, 0, 0, reason);
    }
    
    int handle_go_away(Header& hdr);

    int ping();

    int handle_ping(Header& hdr);

    int send_message(Buffer* hdr_buf, Buffer* body_buf);

    int send_message(Buffer* hdr_buf, const void* body_buf, size_t size);

    int read_body(Buffer* buff, uint32_t length) {
        return buff->ReadNFromSocketStream(socket_stream_, length);
    }

private:
    struct streamInfo {
        acl::fiber_mutex mutex;
        std::unordered_map<uint32_t, MultiplexingStream*> streams;
        std::unordered_set<uint32_t> in_flights;
    };

    struct pingInfo {
        std::unordered_map<uint32_t, std::unique_ptr<acl::fiber_tbox<bool>>> pings;
        uint32_t ping_id;
    };

    friend class MultiplexingStream;

private:
    uint32_t remote_goaway_{0};
    uint32_t local_goaway_{0};
    uint32_t next_stream_id_{0};

    streamInfo mux_streams_{};
    pingInfo   ping_{};

    acl::fiber_tbox<FiberIntCtx> send_chan_{};
    acl::fiber_tbox<bool> recv_done_chan_{};
    acl::fiber_tbox<bool> send_done_chan_{};
    acl::fiber_tbox<bool> keep_alive_chan_{};
    acl::fiber_tbox<bool> keep_alive_done_chan_{};

    acl::fiber_tbox<MultiplexingStream> accept_chn_{};

    bool shut_down_{false};
    std::vector<ACL_FIBER*> fibers_;
    // underlying socket stream
    ISocketStream* socket_stream_;
    bool ownership_{false};
    acl::wait_group wg_{};
    ObjectPool<FiberIntCtx> ctx_pool_{};
    MuxStreamType type_{};
};

class MultiplexingSocketServer : public ISocketServer {
public:
    MultiplexingSocketServer(ISocketServer* underlying, bool ownership)
    : inner_server_(underlying), ownership_(ownership) {}
    ~MultiplexingSocketServer(){ if(ownership_) delete inner_server_; }

    ISocketServer *set_handler(Handler&& handler) override {
        inner_server_->set_handler([&](ISocketStream* stream) -> int{
            return this->handle_connection(stream);
        });
        handler_ = handler;
        return this;
    }

    int init(const std::string &addr, uint16_t port) override{
        return inner_server_->init(addr, port);
    }

    int init(const std::string &path) override {
        return inner_server_->init(path);
    }

    ISocketStream * accept() override {
        return nullptr;
    }

    int start(int io_thread_num = 1) override {
        return inner_server_->start(io_thread_num);
    }

    void stop() override {
        inner_server_->stop();
    }

private:
    virtual int handle_connection(ISocketStream* stream) {
        auto mux_session = new MultiplexingSession(stream, false, MuxStreamType::Server);
        while (true) {
            auto mux_stream = mux_session->accept_stream();
            if (!mux_stream) break;
            LOG(INFO) << "server recv " <<  mux_stream;
            go[&](){
                MultiplexingSocketServer::handler(this->handler_, mux_stream);
            };
        }
        delete mux_session;
        LOG(INFO) << "server session close";
        return 0;
    }

    static void handler(const Handler& m_handler, ISocketStream* stream) {
        (void)m_handler(stream);
        delete stream;
        LOG(INFO) << "server delete stream " <<  stream;
    }
private:
    ISocketServer* inner_server_{nullptr};
    Handler handler_{nullptr};
    bool ownership_{false};
};

class MultiplexingSocketClient : public ISocketClient {
public:
    MultiplexingSocketClient(ISocketClient* underlying, bool ownership)
    : sessions_(), inner_client_(underlying), ownership_(ownership), mutex_() {}

    ~MultiplexingSocketClient() {
        for (const auto& ses : sessions_ ) {
            ses.second->close();
        }
        if (ownership_) delete inner_client_;
    }
public:
    ISocketStream *connect(const std::string &remote, int port) override {
        EndPoint ep;
        if (ep.from(remote, port)) {
            return connect(ep);
        }
        return nullptr;
    }

    ISocketStream *connect(const std::string &path) override {
        return nullptr;
    }

    ISocketStream * connect(EndPoint remote) override {
        mutex_.lock();
        auto it = sessions_.find(remote);
        if (it == sessions_.end()) {
            mutex_.unlock();
            auto conn = inner_client_->connect(remote);
            if (!conn) {
                return nullptr;
            }

            auto mux_session = new MultiplexingSession(conn, true, MuxStreamType::Client);
            auto mux_stream = mux_session->open_stream();
            if (mux_stream == nullptr) {
                return mux_stream;
            }
            mutex_.lock();
            sessions_.emplace(remote, mux_session);
            mutex_.unlock();
            return mux_stream;
        }
        auto mux_stream = it->second->open_stream();
        if (mux_stream == nullptr) {
            sessions_.erase(it);
            return nullptr;
        }
        mutex_.unlock();
        return mux_stream;
    }
private:
    std::unordered_map<EndPoint, MultiplexingSession*> sessions_{};
    ISocketClient* inner_client_{nullptr};
    bool ownership_{false};
    acl::fiber_mutex mutex_{};
};

}}