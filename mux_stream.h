#pragma once
#include <utility>

#include "common.h"
#include "socket_stream.h"
#include "buffer.h"
#include "mux_define.h"


namespace arch_net { namespace mux {

class MultiplexingSession;

class MultiplexingStream : public ISocketStream {
public:

    MultiplexingStream(uint32_t id, MultiplexingSession* session, StreamState state)
        : id_(id), session_(session), recv_window_(FLAGS_MaxStreamWindowSize), send_window_(FLAGS_MaxStreamWindowSize),
          state_(state),state_lock_(), recv_buff_(), control_hdr_(), send_hdr_(), hdr_buff_(HeaderSize, 0),
          recv_done_notify_(false), send_done_notify_(false), establish_chn_(false), close_chn_(), on_received_cb_(){}

    ~MultiplexingStream(){ close(); }

    ssize_t recv(void *buf, size_t count, int flags) override;

    ssize_t recv(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t recv(Buffer* buf) override;

    ssize_t send(const void *buf, size_t count, int flags) override;

    ssize_t send(const struct iovec *iov, int iovcnt, int flags) override;

    ssize_t send(Buffer* buf) override;

    ssize_t sendfile(int in_fd, off_t offset, size_t count) override;

    int get_fd() override;

    int send_window_update();

    int read_data(Header& hdr, uint16_t flags);

    int close() override;

    int increase_send_window(Header& hdr, uint16_t flags);

    int set_close();

    int stream_id() { return id_; }

private:

    uint16_t send_flags();

    int process_flags(uint16_t flags, bool& close_stream);

    void send_close();

    int interrupt();
private:
    uint32_t id_;
    MultiplexingSession* session_;
    uint32_t recv_window_;
    uint32_t send_window_;
    StreamState state_;
    acl::fiber_mutex state_lock_;
    acl::fiber_mutex recv_lock_;
    Buffer recv_buff_;
    Header control_hdr_;
    Header send_hdr_;
    Buffer hdr_buff_;
    acl::fiber_tbox<int> recv_done_notify_;
    acl::fiber_tbox<int> send_done_notify_;
    acl::fiber_tbox<int> establish_chn_;
    acl::fiber_tbox<int> close_chn_;

    acl::wait_group close_wg_;

    OnReceivedCallback on_received_cb_;
};


}}