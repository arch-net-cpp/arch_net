#pragma once
#include "../common.h"
#include "../socket_stream.h"
#include "../buffer.h"
#include <google/protobuf/message.h>
#include "pbrpc_codec.h"

#define WEBSOCKET_HEARTBEAT_INTERVAL 30000

namespace arch_net { namespace robin {

enum {
    FRAME_CONTINUATION = 0x00,
    FRAME_TEXT         = 0x01,
    FRAME_BINARY       = 0x02,
    FRAME_RSV3         = 0x03,
    FRAME_RSV4         = 0x04,
    FRAME_RSV5         = 0x05,
    FRAME_RSV6         = 0x06,
    FRAME_RSV7         = 0x07,
    FRAME_CLOSE        = 0x08,
    FRAME_PING         = 0x09,
    FRAME_PONG         = 0x0A,
    FRAME_CTL_RSVB     = 0x0B,
    FRAME_CTL_RSVC     = 0x0C,
    FRAME_CTL_RSVD     = 0x0D,
    FRAME_CTL_RSVE     = 0x0E,
    FRAME_CTL_RSVF     = 0x0F,
};

struct frame_header {
    bool fin;
    bool rsv1;
    bool rsv2;
    bool rsv3;
    unsigned char opcode:4;
    bool mask;
    unsigned long long payload_len;
    unsigned int masking_key;

    frame_header() {
        fin         = false;
        rsv1        = false;
        rsv2        = false;
        rsv3        = false;
        opcode      = FRAME_TEXT;
        mask        = false;
        payload_len = 0;
        masking_key = 0;
    }
};

class websocket : public TNonCopyable {
public:
    websocket(ISocketStream* stream);
    ~websocket();
    websocket& reset();

    //设置是否结束的标志位
    websocket& set_frame_fin(bool yes);
    //设置保留标志位
    websocket& set_frame_rsv1(bool yes);
    //设置保留标志位
    websocket& set_frame_rsv2(bool yes);
    //设置保留标志位
    websocket& set_frame_rsv3(bool yes);
    //设置数据帧类型
    websocket& set_frame_opcode(unsigned char type);
    //设置本数据帧数据载体的总长度
    websocket& set_frame_payload_len(unsigned long long len);
    //设置数据帧数据的掩码值，客户端模式下必须设置此项
    websocket& set_frame_masking_key(unsigned int mask);
    //生成数据帧数据的掩码值
    uint32_t gen_masking_key() { return rand();}
    // 发送数制帧中的数据体
    bool send_frame_data(Buffer* data);
    //读取数据帧帧头
    bool read_frame_head();
    //读取数据帧数据体
    int read_frame_data(Buffer* recv_buf);

    bool is_head_finish() const;

    const frame_header& get_frame_header() const { return header_; }

    bool frame_is_fin() const { return header_.fin; }

    bool frame_is_rsv1() const { return header_.rsv1; }

    bool frame_is_rsv2() const { return header_.rsv2; }

    bool frame_is_rsv3() const { return header_.rsv3; }

    unsigned char get_frame_opcode() const{return header_.opcode;}

    bool frame_has_mask() const{return header_.mask;}

    unsigned long long get_frame_payload_len() const {return header_.payload_len;}

    unsigned int get_frame_masking_key() const{return header_.masking_key;}

private:
    void make_frame_header();
    void update_head_2bytes(unsigned char ch1, unsigned ch2);

private:
    ISocketStream* stream_;
    struct frame_header header_;
    std::vector<unsigned char> header_buf_;
    size_t header_size_;
    size_t header_len_;
    bool header_sent_;
    unsigned status_;
};

class StreamingConnection {
public:
    StreamingConnection(ISocketStream *stream,  bool server_side = true)
            : stream_(stream), ws_in_(new websocket(stream)), ws_out_(new websocket(stream)),
            opcode_(0), sender_chan_(false), error_chan_(), server_side_(server_side) {}
    ~StreamingConnection();

public:
    bool send(::google::protobuf::Message* message);

    bool recv(::google::protobuf::Message* message);

    bool close_streaming();

    bool process();

private:

    bool recv_loop(Buffer* buffer);

    int close_wc_connection();

    bool sendPong(const std::string& data = "");

    bool sendPing(const std::string& data = "");

private:
    void addTask(std::function<void()> fun) {
        auto ctx = new FiberCtx(std::move(fun));
        sender_chan_.push(ctx);
    }
private:
    ISocketStream *stream_;
    std::unique_ptr<websocket> ws_in_;
    std::unique_ptr<websocket> ws_out_;

    int opcode_;
    acl::fiber_tbox<FiberCtx> sender_chan_;
    acl::fiber_tbox<bool> heartbeat_chan_;
    acl::fiber_tbox<bool> error_chan_;

    bool closed_{false};
    bool remote_close_{false};
    bool server_side_;

    acl::fiber_tbox<Buffer> inbox_{false};
    acl::fiber_tbox<bool> exit_signal_;

    friend class RobinPBrpcConnection;
    friend class ClientSideConnection;
};


static bool is_big_endian(void) {
    const int n = 1;
    if (*(char*) &n) return false;
    return true;
}

#define	hton64(val) is_big_endian() ? val : arch_netbswap_64(val)
#define	ntoh64(val) hton64(val)

}}