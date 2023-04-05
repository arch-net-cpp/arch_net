
#include "mux_session.h"

namespace arch_net  { namespace mux {

MultiplexingSession::MultiplexingSession(ISocketStream *underlying, bool ownership,
                                         MuxStreamType type)
    : remote_goaway_(0), local_goaway_(0), next_stream_id_(0), mux_streams_(),
    ping_(), send_chan_(), recv_done_chan_(), send_done_chan_(), keep_alive_chan_(),
    keep_alive_done_chan_(), accept_chn_(false), socket_stream_(underlying),
    ownership_(ownership), wg_(), ctx_pool_(), type_(type) {

    if (type == MuxStreamType::Client) {
        next_stream_id_ = 1;
    } else {
        next_stream_id_ = 2;
    }
    session_setup();
}

MultiplexingSession::~MultiplexingSession() {
    close();
    if (ownership_) delete socket_stream_;
}

ISocketStream *MultiplexingSession::open_stream() {

    if (is_closed()) {
        return nullptr;
    }

    auto res_chan = std::make_shared<acl::fiber_tbox<ISocketStream>>();
    std::weak_ptr<acl::fiber_tbox<ISocketStream>> weak_chan(res_chan);

    add_send_task([this, weak_chan = std::move(weak_chan)]() -> int {
        auto res_chan = weak_chan.lock();
        if (!res_chan) return -1;

        if (is_closed()) {
            res_chan->push(nullptr);
            return -1 ;
        }

        if (remote_goaway_ == 1) {
            res_chan->push(nullptr);
            return -1;
        }

        uint32_t next_stream_id = next_stream_id_ += 2 ;  //TODO over flow check
        auto mux_stream = std::make_unique<MultiplexingStream>(next_stream_id,
                                                               this, StreamState::StreamInit);

        int ret = mux_stream->send_window_update();
        if (ret <= 0) {
            res_chan->push(nullptr);
            return -1;
        }
        auto stm = mux_stream.release();
        mux_streams_.mutex.lock();
        mux_streams_.streams.emplace(next_stream_id, stm);
        mux_streams_.in_flights.insert(next_stream_id);
        mux_streams_.mutex.unlock();

        res_chan->push(stm);
        return 1;
    });

    bool found;
    auto ret = res_chan->pop(FLAGS_StreamOpenTimeout, &found);
    if (found && !shut_down_) {
        return ret;
    }
    return nullptr;
}

ISocketStream *MultiplexingSession::accept_stream() {
    auto stream = accept_chn_.pop();
    return stream;
}

void MultiplexingSession::session_setup() {
    wg_.add(3);
    (void)create_fiber([&]() {
        auto send_loop = [&](){
            while (true) {
                auto ctx = send_chan_.pop();
                if (!ctx) return 1;
                defer(ctx_pool_.Release(ctx));
                if (ctx->fn_() <= 0) return -1;
            }
        };

        int ret = send_loop();
        wg_.done();
        if (ret <= 0) {
            exit_err();
        }
        LOG(INFO) << "type: "<< type_ << "MultiplexingSession send fiber done";
        return 0;
    });

    (void) create_fiber([&]() {
        int ret = recv_loop();
        wg_.done();
        if (ret <= 0 ) {
            exit_err();
        }
        LOG(INFO) << "type: "<< type_ << "MultiplexingSession recv fiber done";

        return 0;
    });

    (void) create_fiber([&] (){
        auto keep_alive_loop = [&]() {
            while (true) {
                bool found = false;
                keep_alive_chan_.pop(FLAGS_KeepAliveInterval, &found);
                if (found) return 1;
                if (ping() <= 0) return -1;
            }
        };
        int ret = keep_alive_loop();
        wg_.done();
        if (ret <= 0) {
            exit_err();
        }
        LOG(INFO) << "type: "<< type_ << "MultiplexingSession keepalive fiber done";

        return 0;
    });
}

void MultiplexingSession::add_send_task(std::function<int()> func) {
    auto ctx = ctx_pool_.Get();
    ctx->fn_ = std::move(func);

    send_chan_.push(ctx);
}

int MultiplexingSession::recv_loop() {
    Header hdr;
    Buffer recv_buf;
    while (!is_closed()) {
        if (type_ == 0) {
            int a;
            a++;
        }
        int ret = hdr.recv_and_decode(&recv_buf, socket_stream_);
        if (ret <= 0) {
            return -1;
        }
        if (is_closed()) {
            return 1;
        }
        LOG(INFO) << "type: "<< type_ <<" recv hdr message: "<< hdr.to_string();
        switch ((MsgType)hdr.msg_type()) {
            case typeData:
                ret = handle_stream_message(hdr);
                break;
            case typeWindowUpdate:
                ret = handle_stream_message(hdr);
                break;
            case typePing:
                ret = handle_ping(hdr);
                break;
            case typeGoAway:
                ret = handle_go_away(hdr);
                break;
        }
        if (ret <= 0) {
            return -1;
        }
    }
    return 1;
}

int MultiplexingSession::handle_stream_message(Header &hdr) {
    uint32_t id = hdr.stream_id();
    auto flags = hdr.flags();

    if ((flags & flagSYN) == flagSYN) {
        int ret = incoming_stream(id);
        if (ret <= 0) {
            return -1;
        }
    }
    mux_streams_.mutex.lock();
    defer(mux_streams_.mutex.unlock());

    auto it = mux_streams_.streams.find(id);
    if (it == mux_streams_.streams.end()) {
        if (hdr.msg_type() == typeData && hdr.length() > 0) {
            Buffer discard(hdr.length());
            discard.ReadNFromSocketStream(socket_stream_, hdr.length());
        }
        return 1;
    }
    // window update
    auto stream = it->second;
    if (hdr.msg_type() == typeWindowUpdate) {
        int ret = stream->increase_send_window(hdr, flags);
        if (ret <= 0) {
            add_send_task([&]() {
                Buffer hdr_buff(HeaderSize,0);
                Header hdr;
                go_away(hdr, hdr_buff, goAwayProtoErr);
                return send_message(&hdr_buff, nullptr);
            });
            return -1;
        }
        return 1;
    }

    // read the data
    if (stream->read_data(hdr, flags) <= 0) {
        // send error to peer
        add_send_task([&]() {
            Buffer hdr_buff(HeaderSize,0);
            Header hdr;
            go_away(hdr, hdr_buff, goAwayProtoErr);
            return send_message(&hdr_buff, nullptr);
        });

        return -1;
    }

    return 1;
}

int MultiplexingSession::incoming_stream(uint32_t id) {
    if (local_goaway_) {
        add_send_task([&]() {
            Buffer hdr_buff(HeaderSize,0);
            Header hdr;
            hdr.encode(&hdr_buff, typeWindowUpdate, flagRST, id, 0);
            return send_message(&hdr_buff, nullptr);
        });
        return 1;
    }
    bool found_same_id = false;
    mux_streams_.mutex.lock();
    found_same_id = mux_streams_.streams.find(id) != mux_streams_.streams.end();
    mux_streams_.mutex.unlock();

    if (found_same_id) {
        // send error back
        add_send_task([&]() {
            Buffer hdr_buff(HeaderSize,0);
            Header hdr;
            go_away(hdr, hdr_buff, GoAwayType::goAwayProtoErr);
            return send_message(&hdr_buff, nullptr);
        });
        return -1;
    }

    auto mux_stream = new MultiplexingStream(id, this, StreamState::StreamSYNReceived);
    int ret = mux_stream->send_window_update();
    if (ret <= 0) {
        return -1;
    }
    mux_streams_.mutex.lock();
    mux_streams_.streams.emplace(id, mux_stream);
    mux_streams_.mutex.unlock();

    accept_chn_.push(mux_stream);

    return 1;
}

void MultiplexingSession::close_stream(uint32_t id) {
    mux_streams_.mutex.lock();
    defer(mux_streams_.mutex.unlock());
    mux_streams_.streams.erase(id);

}

int MultiplexingSession::handle_ping(Header &hdr) {
    if ((hdr.flags() & flagSYN) == flagSYN) {
        // send
        add_send_task([&]() {
            Buffer hdr_buf(HeaderSize, 0);
            Header new_hdr;
            new_hdr.encode(&hdr_buf, typePing, flagACK, 0, hdr.length());
            return send_message(&hdr_buf, nullptr);
        });
        return 1;
    }

    uint32_t ping_id = hdr.length();
    auto it = ping_.pings.find(ping_id);
    if (it != ping_.pings.end()) {
        it->second.get()->push(nullptr);
    }
    return 1;
}

int MultiplexingSession::handle_go_away(Header &hdr) {
    auto code = hdr.length();
    switch (code) {
        case goAwayNormal:
            remote_goaway_ = 1;
            return 1;
        case goAwayProtoErr:
            return -1; //s.logger.Printf("[ERR] yamux: received protocol error go away")
        case goAwayInternalErr:
            return -1; //s.logger.Printf("[ERR] yamux: received internal error go away")
        default:
            return -1; //s.logger.Printf("[ERR] yamux: received unexpected go away")
    }
}

int MultiplexingSession::ping() {
    uint32_t id = ping_.ping_id;
    ping_.ping_id++;
    ping_.pings.emplace(id, std::make_unique<acl::fiber_tbox<bool>>());

    add_send_task([&]() {
        Buffer hdr_buf(HeaderSize, 0);
        Header hdr;
        hdr.encode(&hdr_buf, typePing, flagSYN, 0, id);
        return send_message(&hdr_buf, nullptr);
    });


    bool found;
    ping_.pings[id].get()->pop(FLAGS_ConnectionWriteTimeout, &found);
    if (found) {
        ping_.pings.erase(id);
        return 1;
    }
    return -1;
}

int MultiplexingSession::send_message(Buffer *hdr_buf, const void *body_buf, size_t size) {
    if (shut_down_) {
        return -1;
    }

    if (hdr_buf != nullptr) {
        auto n = socket_stream_->send(hdr_buf->data(), hdr_buf->size());
        if (n <= 0) {
            return -1;
        }
        hdr_buf->Retrieve(n);
    }

    if (body_buf != nullptr) {
        auto n = socket_stream_->send(body_buf, size);
        if (n <= 0) {
            return -1;
        }
    }
    return 1;
}

int MultiplexingSession::send_message(Buffer *hdr_buf, Buffer *body_buf) {
    if (shut_down_) {
        return -1;
    }

    if (hdr_buf != nullptr) {
        auto n = socket_stream_->send(hdr_buf->data(), hdr_buf->size());
        if (n <= 0) {
            return -1;
        }
        hdr_buf->Retrieve(n);
    }

    if (body_buf != nullptr) {
        auto n = socket_stream_->send(body_buf->data(), body_buf->size());
        if (n <= 0) {
            return -1;
        }
        body_buf->Retrieve(n);
    }
    return 1;
}

void MultiplexingSession::close() {
    if (shut_down_) return;

    shut_down_ = true;

    mux_streams_.mutex.lock();
    for (auto & stream : mux_streams_.streams) {
        stream.second->set_close();
    }
    mux_streams_.mutex.unlock();

    send_chan_.push(nullptr);

    socket_stream_->close();

    keep_alive_chan_.push(nullptr);

    wg_.wait();
    accept_chn_.push(nullptr);

}
}}