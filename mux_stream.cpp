#include "mux_stream.h"
#include "mux_session.h"

namespace arch_net { namespace mux {

ssize_t MultiplexingStream::recv(Buffer *buff) {
    auto n = recv(buff->WriteBegin(), buff->WritableBytes(), 0);
    if (n > 0) {
        buff->WriteBytes(n);
    }
    return n;
}

ssize_t MultiplexingStream::recv(void *buf, size_t count, int flags) {

    close_wg_.add(1);
    defer(close_wg_.done());

    switch (state_) {
        case StreamLocalClose:
        case StreamRemoteClose:
        case StreamClosed:
        case StreamReset:
            recv_lock_.lock();
            if (recv_buff_.size() == 0) {
                recv_lock_.unlock();
                return 0;
            }
            recv_lock_.unlock();
        default:
            break;
    }

    while (true) {
        recv_lock_.lock();
        if (recv_buff_.size() == 0) {
            recv_lock_.unlock();

            auto res = recv_done_notify_.pop();
            if (!res) {
                return -1;
            }

            recv_lock_.lock();
            if (recv_buff_.size() == 0) {
                recv_lock_.unlock();
                continue;
            }

            auto slice = recv_buff_.Next(count);
            std::memcpy(buf, slice.data(), slice.size());
            recv_lock_.unlock();
            return slice.size();
        } else {
            auto slice = recv_buff_.Next(count);
            std::memcpy(buf, slice.data(), slice.size());
            recv_lock_.unlock();
            return slice.size();
        }
    }

}

ssize_t MultiplexingStream::recv(const struct iovec *iov, int iovcnt, int flags) {
    throw "unsupported method";
    return -1;
}

ssize_t MultiplexingStream::send(Buffer* buff) {
    auto n = send(buff->data(), buff->size(), 0);
    if (n > 0) {
        buff->Retrieve(n);
    }
    return n;
}

ssize_t MultiplexingStream::send(const void *buf, size_t count, int flags) {
    close_wg_.add(1);
    defer(close_wg_.done());

    switch (state_) {
        case StreamLocalClose:
        case StreamRemoteClose:
        case StreamClosed:
        case StreamReset:
            close_chn_.push(nullptr);
            return -1;
        default:
            break;
    }

    if (send_window_ == 0) {
        return 0;
    }

    session_->add_send_task([this, buf=buf, count=count]() -> int {

        switch (state_) {
            case StreamLocalClose:
            case StreamRemoteClose:
            case StreamClosed:
            case StreamReset:
                send_done_notify_.push(nullptr);
                return 1;
            default:
                break;
        }

        auto s_flags = send_flags();

        Buffer hdr_buff(HeaderSize,0);
        Header hdr;

        hdr.encode(&hdr_buff, typeData, s_flags, id_, count);
        int ret = session_->send_message(&hdr_buff, buf, count);
        if (ret <= 0) {
            send_done_notify_.push(nullptr);
            return -1;
        }
        send_done_notify_.push(&SendSuccess);
        return 1;
    });

    auto ret = send_done_notify_.pop(FLAGS_ConnectionWriteTimeout);
    if (ret) {
        return count;
    }
    // timeout, close stream
    return -1;
}

ssize_t MultiplexingStream::send(const struct iovec *iov, int iovcnt, int flags) {
    throw "unsupported method";
    return -1;
}

ssize_t MultiplexingStream::sendfile(int in_fd, off_t offset, size_t count) {
    throw "unsupported method";
    return -1;
}

int MultiplexingStream::get_fd() {
    return -1;
}

int MultiplexingStream::send_window_update() {
    uint32_t max = FLAGS_MaxStreamWindowSize;
    uint32_t delta = max - recv_buff_.size() - recv_window_;
    uint16_t flags = send_flags();
    if (delta < (max / 2) && flags == 0) {
        return 1;
    }
    recv_window_ += delta;
    control_hdr_.encode(&hdr_buff_, MsgType::typeWindowUpdate, flags, id_, delta);
    int n = session_->send_message(&hdr_buff_, nullptr);
    if (n <= 0) {
        return n;
    }
    return 1;
}

uint16_t MultiplexingStream::send_flags() {
    uint16_t flags = 0;
    switch (state_) {
        case StreamInit:
            flags |= Flags::flagSYN;
            state_ = StreamState::StreamSYNSent;
            break;
        case StreamSYNReceived:
            flags |= Flags::flagACK;
            state_ = StreamState::StreamEstablished;
            break;
        default:
            break;
    }
    return flags;
}

void MultiplexingStream::send_close() {
    auto flags = send_flags();
    flags |= flagFIN;
    acl::fiber_tbox<bool> chan;
    session_->add_send_task([&]() {
        Buffer hdr_buff(HeaderSize,0);
        Header hdr;
        hdr.encode(&hdr_buff, typeWindowUpdate, flags, id_, 0);
        defer(chan.push(nullptr));
        return session_->send_message(&hdr_buff, nullptr);
    });
    chan.pop();
}

int MultiplexingStream::close() {

    state_lock_.lock();

    switch (state_) {
        // Opened means we need to signal a close
        case StreamSYNSent:
        case StreamSYNReceived:
        case StreamEstablished:
            state_ = StreamLocalClose;
            send_close();
            recv_done_notify_.push(nullptr);
            send_done_notify_.push(nullptr);
            state_ = StreamClosed;
            state_lock_.unlock();

            close_wg_.wait();
            session_->close_stream(id_);
            return 1;

        case StreamLocalClose:
            state_ = StreamClosed;
            state_lock_.unlock();

            session_->close_stream(id_);
            return 1;

        case StreamRemoteClose:
            state_ = StreamClosed;
            recv_done_notify_.push(nullptr);
            send_done_notify_.push(nullptr);
            state_lock_.unlock();

            close_wg_.wait();
            session_->close_stream(id_);
            return 1;

        case StreamClosed:
        case StreamReset:
            state_lock_.unlock();

            return 1;
        default:
            LOG(ERROR)<< "unhandled state";
            state_lock_.unlock();

            return -1;
    }
}

int MultiplexingStream::set_close() {
    state_lock_.lock();
    state_ = StreamClosed;
    state_lock_.unlock();

    recv_done_notify_.push(nullptr);
    send_done_notify_.push(nullptr);
    return 0;
}

int MultiplexingStream::interrupt() {

    return 1;
}

int MultiplexingStream::increase_send_window(Header &hdr, uint16_t flags) {
    bool close_stream = false;
    if (process_flags(flags, close_stream) <= 0) {
        return -1;
    }
    send_window_ += hdr.length();

    if (close_stream) {
        // peer send FIN
        recv_done_notify_.push(nullptr);
        send_done_notify_.push(nullptr);
    }
    return 1;
}

int MultiplexingStream::process_flags(uint16_t flags, bool& close_stream) {
    state_lock_.lock();
    defer(state_lock_.unlock());

    if ( (flags & flagACK) == flagACK) {
        if (state_ == StreamSYNSent) {
            state_ = StreamEstablished;
        }
    }

    if ( (flags & flagFIN) == flagFIN) {
		switch (state_) {
		case StreamSYNSent:
		case StreamSYNReceived:
		case StreamEstablished:
            state_ = StreamRemoteClose;
            close_stream = true;
            break;
		case StreamLocalClose:
			//state_ = StreamClosed;
            close_stream = true;
            break;
		default:
			LOG(ERROR)<< "unexpected FIN flag in state: " << state_;
			return -1;
		}
	}

    if ( (flags & flagRST) == flagRST) {
            state_ = StreamReset;
            close_stream = true;
    }

    return 1;
}

int MultiplexingStream::read_data(Header &hdr, uint16_t flags) {
    bool close_stream = false;
    if (process_flags(flags, close_stream) <= 0) {
        return -1;
    }

    uint32_t length = hdr.length();
    if (length > recv_window_) {
        return -1;
    }
    recv_lock_.lock();
    uint32_t ret = session_->read_body(&recv_buff_, length);
    if (ret <= 0) {
        return -1;
    }
    recv_lock_.unlock();
    recv_window_ -= ret;

    assert(ret == length);
    if (ret > 0) {
        recv_done_notify_.push(&RecvSuccess);
    }

    if (close_stream) {
        recv_done_notify_.push(nullptr);
        send_done_notify_.push(nullptr);
    }

    return 1;
}

}}