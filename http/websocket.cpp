#include "websocket.h"

namespace arch_net { namespace coin {

enum {
    WS_HEAD_2BYTES,
    WS_HEAD_LEN_2BYTES,
    WS_HEAD_LEN_8BYTES,
    WS_HEAD_MASKING_KEY,
    WS_HEAD_FINISH,
};

websocket::websocket(ISocketStream* stream) : stream_(stream) , header_buf_(), header_size_(0),
    header_len_(0),header_sent_(false), status_(WS_HEAD_2BYTES) {

    reset();
}

websocket::~websocket() {}

websocket& websocket::reset() {
    header_.fin         = false;
    header_.rsv1        = false;
    header_.rsv2        = false;
    header_.rsv3        = false;
    header_.opcode      = FRAME_TEXT;
	header_.mask        = false;
    header_.payload_len = 0;
	header_.masking_key = 0;

    header_sent_        = false;
    status_             = WS_HEAD_2BYTES;

    header_buf_.clear();
    return *this;
}

websocket& websocket::set_frame_fin(bool yes) {
    header_.fin = yes;
    return *this;
}

websocket& websocket::set_frame_rsv1(bool yes) {
    header_.rsv1 = yes;
    return *this;
}

websocket& websocket::set_frame_rsv2(bool yes) {
    header_.rsv2 = yes;
    return *this;
}

websocket& websocket::set_frame_rsv3(bool yes){
    header_.rsv3 = yes;
    return *this;
}

websocket& websocket::set_frame_opcode(unsigned char type){
    header_.opcode = type;
    return *this;
}

websocket& websocket::set_frame_payload_len(unsigned long long len){
    header_.payload_len = len;
    return *this;
}

websocket& websocket::set_frame_masking_key(unsigned int mask) {
    header_.masking_key = mask;
    header_.mask = mask != 0;
    return *this;
}

void websocket::make_frame_header() {
    header_len_ = 2;
    if (header_.payload_len > 65535) {
        header_len_ += 8;
    } else if (header_.payload_len >= 126) {
        header_len_ += 2;
    }
    if (header_.mask) {
        header_len_ += 4;
    }

    header_buf_.resize(header_len_);
    header_size_ = header_len_;

    if (header_.fin) {
        header_buf_[0] = 0x80;
    } else {
        header_buf_[0] = 0x00;
    }

    header_buf_[0] |= header_.opcode;

    if (header_.payload_len >= 0 && header_.mask) {
        header_buf_[1] = 0x80;
    } else {
        header_buf_[1] = 0x00;
    }

    unsigned long long offset = 1;
    unsigned long long payload_len = header_.payload_len;

    if (payload_len <= 125) {
        header_buf_[offset++] |= payload_len & 0xff;
    } else if (payload_len <= 65535) {
        header_buf_[offset++] |= 126;
        header_buf_[offset++] = (unsigned char) (payload_len >> 8) & 0xff;
        header_buf_[offset++] = (unsigned char) payload_len & 0xff;
    } else {
        header_buf_[offset++] |= 127;
        header_buf_[offset++] = (unsigned char) ((payload_len >> 56) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 48) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 40) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 32) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 24) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 16) & 0xff);
        header_buf_[offset++] = (unsigned char) ((payload_len >> 8) & 0xff);
        header_buf_[offset++] = (unsigned char) (payload_len & 0xff);
    }

    if (payload_len >= 0 && header_.mask) {
        unsigned int masking_key = header_.masking_key;
        header_buf_[offset++] = (unsigned char) ((masking_key >> 24) & 0xff);
        header_buf_[offset++] = (unsigned char) ((masking_key >> 16) & 0xff);
        header_buf_[offset++] = (unsigned char) ((masking_key >> 8) & 0xff);
        header_buf_[offset++] = (unsigned char) (masking_key & 0xff);

        // save result in masking_key for send_frame_data
        memcpy(&header_.masking_key, &header_buf_[0] + offset - 4, 4);
    }
}


bool websocket::send_frame_data(Buffer* buffer) {
    if (!header_sent_) {
        header_sent_ = true;
        make_frame_header();
        if (stream_->send(&header_buf_[0], header_len_) == -1) {
            //logger_error("write header error %s, len: %d"
            return false;
        }
    }

    if (buffer == nullptr || buffer->size() == 0) {
        return true;
    }

    if (header_.mask) {
        unsigned char* mask = (unsigned char*) &header_.masking_key;
        for (size_t i = 0; i < buffer->size(); i++) {
            const_cast<char*>(buffer->data())[i] ^= mask[(i) % 4];
        }
    }

    if (stream_->send(buffer) == -1) {
        //logger_error("write frame data error %s", last_serror());
        return false;
    }
    return true;
}

bool websocket::read_frame_head() {
    reset();
    unsigned char buf[8];
    if (stream_->recv(buf, 2) == -1) {
        return false;
    }

    update_head_2bytes(buf[0], buf[1]);

    size_t count;
    // payload_len: <= 125 | 126 | 127

    if (header_.payload_len == 126) {
        count = 2;
    } else if (header_.payload_len > 126) {
        count = 8;
    } else {
        count = 0;
    }

    if (count > 0) {
        int ret;
        if ((ret = stream_->recv(buf, count)) == -1) {
            return false;
        } else if (ret == 2) {
            unsigned short n;
            memcpy(&n, buf, ret);
            header_.payload_len = ntohs(n);
        } else {
            // ret == 8
            memcpy(&header_.payload_len, buf, ret);
            header_.payload_len = ntoh64(header_.payload_len);
        }
    }

    if (!header_.mask) {
        return true;
    }

    if (stream_->recv(&header_.masking_key, sizeof(unsigned int)) == -1) {
        return false;
    }

    return true;
}

int websocket::read_frame_data(Buffer* recv_buf) {
    auto ret = recv_buf->ReadNFromSocketStream(stream_, header_.payload_len);
    if (ret <= 0) {
        return -1;
    }

    if (header_.mask) {
        auto *mask = (unsigned char *) &header_.masking_key;
        for (int i = 0; i < ret; i++) {
            const_cast<char *>(recv_buf->data())[i] ^= mask[(i) % 4];
        }
    }
    return ret;
}

void websocket::update_head_2bytes(unsigned char ch1, unsigned ch2)
{
    header_.fin         = (ch1 >> 7) & 0x01;
    header_.rsv1        = (ch1 >> 6) & 0x01;
    header_.rsv2        = (ch1 >> 5) & 0x01;
    header_.rsv3        = (ch1 >> 4) & 0x01;
    header_.opcode      = ch1 & 0x0f;
    header_.mask        = (ch2 >> 7) & 0x01;

    header_.payload_len = ch2 & 0x7f;
}


bool websocket::is_head_finish() const {
    return status_ == WS_HEAD_FINISH;
}

bool WebSocketConnection::process() {
    callback_.OnOpen(this);

    acl::wait_group wg;
    wg.add(4);
    (void)create_fiber([&]()->int {
        auto buffer = GlobalBufferPool::getInstance().Get();
        defer(GlobalBufferPool::getInstance().Release(buffer););
        while (true) {
            if (!recv_loop(buffer)) {
                break;
            }
        }
        wg.done();
        close_wc_connection();
        LOG(INFO) << "recv fiber exit";
        return 0;
    });

    (void)create_fiber([&]()->int{
        while (true) {
            auto ctx = sender_chan_.pop();
            if (!ctx) break;

            defer(delete ctx;);
            ctx->fn_();

        }
        wg.done();
        close_wc_connection();
        LOG(INFO) << "sender fiber exit";

        return 0;
    });

    (void)create_fiber([&]()->int{
        while (true) {
            bool found;
            heartbeat_chan_.pop(WEBSOCKET_HEARTBEAT_INTERVAL, &found);
            if (found) {
                break;
            }
            // heartbeat
            sendPing();
        }
        wg.done();
        close_wc_connection();
        closed_ = true;
        LOG(INFO) << "heartbeat fiber exit";

        return 0;
    });
    // when async worker thread has an error, this channel will recv message
    (void) create_fiber([&]()->int{
        error_chan_.pop();

        wg.done();
        close_wc_connection();
        LOG(INFO) << "global error fiber exit";

        return 0;
    });

    wg.wait();
    return true;
}

int WebSocketConnection::close_wc_connection() {
    if (closed_) return 0;

    closed_ = true;
    close(stream_->get_fd());
    sender_chan_.push(nullptr);
    heartbeat_chan_.push(nullptr);
    error_chan_.push(nullptr);
    return 0;
}

bool WebSocketConnection::recv_loop(Buffer *buffer) {
    // read websocket header
    if (!ws_in_->read_frame_head()) {
        return false;
    }

    int  opcode = ws_in_->get_frame_opcode();
    bool ret    = false;

    bool finish = ws_in_->frame_is_fin();
    if (finish) {
        if (opcode == FRAME_CONTINUATION) {
            // restore the saved opcode
            opcode = opcode_;
        }
    } else if (opcode != FRAME_CONTINUATION) {
        // save opcode
        opcode_ = opcode;
    }

    switch (opcode) {
        case FRAME_PING:
            ret = callback_.OnPing(this);
            break;
        case FRAME_PONG:
            ret = callback_.OnPong(this);
            break;
        case FRAME_CLOSE:
            callback_.OnClose(this);
            return false;
        case FRAME_TEXT:
            if (ws_in_->read_frame_data(buffer) <= 0) {
                return false;
            }
            if (finish) {
                LOG(INFO) << "received full message, async run callback";
                auto new_buffer = GlobalBufferPool::getInstance().Get();
                new_buffer->Swap(*buffer);
                auto weak_conn = std::weak_ptr<WebSocketConnection>(shared_from_this());
                worker_pool_->addTask([weak_conn, buffer = new_buffer] {
                    defer(GlobalBufferPool::getInstance().Release(buffer););
                    auto conn = weak_conn.lock();
                    if (!conn) return;
                    if (!conn->callback_.OnMessage(conn.get(), buffer, true)) {
                        conn->error_chan_.push(nullptr);
                    }
                });
                LOG(INFO) << "recv next loop";
                buffer->Reset();
            }
            ret = true;
            break;
        case FRAME_BINARY:
            if (ws_in_->read_frame_data(buffer) <= 0) {
                return false;
            }
            if (finish) {
                auto new_buffer = GlobalBufferPool::getInstance().Get();
                new_buffer->Swap(*buffer);
                auto weak_conn = std::weak_ptr<WebSocketConnection>(shared_from_this());
                worker_pool_->addTask([weak_conn = std::move(weak_conn), buffer = new_buffer] {
                    defer(GlobalBufferPool::getInstance().Release(buffer););
                    auto conn = weak_conn.lock();
                    if (!conn) return;
                    if (!conn->callback_.OnMessage(conn.get(), buffer, false)) {
                        conn->error_chan_.push(nullptr);
                    }
                });
                buffer->Reset();
            }
            ret = true;
            break;
        default:
            //logger_error("unknown websocket frame opcode: %d", opcode);
            ret = false;
            break;
    }

    if (finish) {
        opcode_ = 0;
    }
    return ret;
}

bool WebSocketConnection::sendBinary(const std::string& data) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(data);

    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_BINARY);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::sendBinary(Buffer *buffer) {
    addTask([this, buffer = buffer]() {
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_BINARY);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::sendText(Buffer* buffer) {
    addTask([this, buffer = buffer]() {
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_TEXT);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::sendText(const std::string &text) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(text);
    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_TEXT);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::sendPong(const std::string& data) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(data);

    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_PONG);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::sendPing(const std::string& data) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(data);
    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_PING);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool WebSocketConnection::closeWebsocket() {
    addTask([this]() {
        auto buffer = GlobalBufferPool::getInstance().Get();
        buffer->Reset();
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_CLOSE);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        if (!server_side_) {
            ws_out_->set_frame_masking_key(ws_out_->gen_masking_key());
        }
        ws_out_->send_frame_data(buffer);
        return false;
    });
    return true;
}

}}
