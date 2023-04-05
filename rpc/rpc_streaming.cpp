#include "rpc_streaming.h"

namespace arch_net { namespace robin {

enum {
    WS_HEAD_2BYTES,
    WS_HEAD_LEN_2BYTES,
    WS_HEAD_LEN_8BYTES,
    WS_HEAD_MASKING_KEY,
    WS_HEAD_FINISH,
};

websocket::websocket(ISocketStream* stream)
: stream_(stream) , header_buf_(), header_size_(0),
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
    Buffer recv_buf(8, 0);
    if (recv_buf.ReadNFromSocketStream(stream_, 2) <= 0) {
        return false;
    }
    auto ch1 = recv_buf.ReadUInt8();
    auto ch2 = recv_buf.ReadUInt8();
    update_head_2bytes(ch1, ch2);

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
        if ((ret = recv_buf.ReadNFromSocketStream(stream_, count)) <= 0) {
            return false;
        } else if (ret == 2) {
            header_.payload_len = recv_buf.ReadUInt16();
        } else {
            header_.payload_len = recv_buf.ReadInt64();
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

StreamingConnection::~StreamingConnection() {
    close_streaming();
    exit_signal_.pop();
}

bool StreamingConnection::process() {
    acl::wait_group wg;
    wg.add(4);
    (void)create_fiber([&]()->int {
        auto buffer = GlobalBufferPool::getInstance().Get();
        buffer->Reset();
        defer(GlobalBufferPool::getInstance().Release(buffer););
        while (true) {
            if (!recv_loop(buffer)) {
                break;
            }
        }
        wg.done();
        close_wc_connection();
        LOG(INFO)<< server_side_ << " recv fiber exit";
        return 0;
    });

    (void)create_fiber([&]()->int{
        while (true) {
            auto ctx = sender_chan_.pop();
            if (!ctx) break;

            defer(delete ctx;);
            LOG(INFO)<< server_side_  << " sender work";
            ctx->fn_();

        }
        wg.done();
        close_wc_connection();
        LOG(INFO)<< server_side_  << " sender fiber exit";

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
        LOG(INFO)<< server_side_  << " heartbeat fiber exit";

        return 0;
    });
    // when async worker thread has an error, this channel will recv message
    (void) create_fiber([&]()->int{
        error_chan_.pop();

        wg.done();
        close_wc_connection();
        LOG(INFO)<< server_side_  << " global error fiber exit";

        return 0;
    });

    wg.wait();
    exit_signal_.push(nullptr);
    return true;
}

int StreamingConnection::close_wc_connection() {
    if (closed_) return 0;

    closed_ = true;
    remote_close_ = true;
    inbox_.push(nullptr);

    close(stream_->get_fd());
    sender_chan_.push(nullptr);
    heartbeat_chan_.push(nullptr);
    error_chan_.push(nullptr);
    return 0;
}

bool StreamingConnection::recv_loop(Buffer *buffer) {
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
            ret = true;
            LOG(INFO)<< server_side_  << " on ping";
            break;
        case FRAME_PONG:
            ret = true;
            LOG(INFO)<< server_side_  << " on pong";
            break;
        case FRAME_CLOSE:
            //callback_.OnClose(this);
            remote_close_ = true;
            inbox_.push(nullptr);
            LOG(INFO)<< server_side_  << " on close";
            return false;
        case FRAME_TEXT:
            if (ws_in_->read_frame_data(buffer) <= 0) {
                return false;
            }
            if (finish) {
                LOG(INFO)<< server_side_  << " received full message, async run callback";
                auto new_buffer = GlobalBufferPool::getInstance().Get();
                new_buffer->Swap(*buffer);

                inbox_.push(new_buffer);

                LOG(INFO)<< server_side_  << " recv next loop";
                buffer->Reset();
            }
            ret = true;
            break;
        case FRAME_BINARY:
            if (ws_in_->read_frame_data(buffer) <= 0) {
                return false;
            }
            if (finish) {
                LOG(INFO)<< server_side_  << " received full message, async run callback";
                auto new_buffer = GlobalBufferPool::getInstance().Get();
                new_buffer->Swap(*buffer);

                inbox_.push(new_buffer);

                LOG(INFO)<< server_side_  << " recv next loop";
                buffer->Reset();
            }
            ret = true;
            break;
        default:
            LOG(ERROR) << "unknown websocket frame opcode:" << opcode;
            ret = false;
            break;
    }

    if (finish) {
        opcode_ = 0;
    }
    return ret;
}

bool StreamingConnection::send(::google::protobuf::Message *message) {
    if (remote_close_) {
        return false;
    }
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();

    if (!PBCodec::StreamingEncode(message, buffer)) {
        return false;
    }

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

bool StreamingConnection::recv(::google::protobuf::Message *message) {

    auto buffer = inbox_.pop();
    if (!buffer) {
        return false;
    }
    defer(GlobalBufferPool::getInstance().Release(buffer));
    return PBCodec::StreamingDecode(buffer, message);
}

bool StreamingConnection::close_streaming() {
    addTask([this]() {
        auto buffer = GlobalBufferPool::getInstance().Get();
        buffer->Reset();
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_CLOSE);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        //if (!server_side_) {
        //    ws_->set_frame_masking_key(ws_->gen_masking_key());
        //}
        ws_out_->send_frame_data(buffer);
        return false;
    });
    return true;
}

bool StreamingConnection::sendPong(const std::string& data) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(data);

    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_PONG);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        //if (!server_side_) {
        //    ws_->set_frame_masking_key(ws_->gen_masking_key());
        //}
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

bool StreamingConnection::sendPing(const std::string& data) {
    auto buffer = GlobalBufferPool::getInstance().Get();
    buffer->Reset();
    buffer->Append(data);
    addTask([this, buffer = buffer]() {
        defer(GlobalBufferPool::getInstance().Release(buffer));
        ws_out_->reset();
        ws_out_->set_frame_opcode(FRAME_PING);
        ws_out_->set_frame_fin(true);
        ws_out_->set_frame_payload_len(buffer->size());
        //if (!server_side_) {
        //    ws_->set_frame_masking_key(ws_->gen_masking_key());
        //}
        return ws_out_->send_frame_data(buffer);
    });
    return true;
}

}}