#pragma once
#include <utility>

#include "../common.h"
#include "request.h"

const int WEBSOCKET_HEARTBEAT_INTERVAL = 3000;

namespace arch_net { namespace coin {

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

class WebSocketConnection;
typedef WebSocketConnection* WebSocketConnPtr;

static bool ws_callback(WebSocketConnPtr){ return true; }
static bool auth_callback(const HttpRequest&){ return true; }

class WebSocketCallback {
public:
    WebSocketCallback() {
        OnAuth = auth_callback;
        OnOpen = ws_callback;
        OnError = ws_callback;
        OnClose = ws_callback;
        OnPing = ws_callback;
        OnPong = ws_callback;
    }
    std::function<bool(WebSocketConnPtr, Buffer*, bool)> OnMessage;
    std::function<bool(const HttpRequest&)> OnAuth;
    std::function<bool(WebSocketConnPtr)> OnOpen;
    std::function<bool(WebSocketConnPtr)> OnError;
    std::function<bool(WebSocketConnPtr)> OnClose;
    std::function<bool(WebSocketConnPtr)> OnPing;
    std::function<bool(WebSocketConnPtr)> OnPong;
    void SetCtx(void* ctx) { Ctx = ctx; }
    void* Ctx;
};

class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection>{
public:
    WebSocketConnection(ISocketStream *stream, WebSocketCallback& callback, WorkerPool* pool, bool server_side = true)
    : stream_(stream),ws_in_(new websocket(stream)), ws_out_(new websocket(stream)), opcode_(0),
    sender_chan_(false), callback_(callback), worker_pool_(pool), error_chan_(), server_side_(server_side) {}

public:
    bool process();

    bool sendBinary(Buffer* buff);

    bool sendBinary(const std::string& data);

    bool sendText(Buffer* buff);

    bool sendText(const std::string& text);

    bool sendPong(const std::string& data = "");

    bool sendPing(const std::string& data = "");

    bool closeWebsocket();

private:
    bool recv_loop(Buffer* buffer);
    int close_wc_connection();
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
    WebSocketCallback& callback_;
    WorkerPool* worker_pool_;
    acl::fiber_tbox<bool> error_chan_;

    bool closed_{false};
    bool server_side_;
};


class CoinWebsocket : public TNonCopyable {
public:
    CoinWebsocket(): workers_(new IOWorkerPool){}

    WebSocketCallback& websocket(const std::string& path) {
        return callback_map_[path];
    }

    void websocket(const std::string& path, WebSocketCallback& callback) {
        if (!check_callback(callback)) {
            throw std::invalid_argument("websocket endpoint has incomplete callback");
        }
        callback_map_[path] = callback;
    }

    virtual ~CoinWebsocket() = default;
protected:
    int handle_websocket(const HttpRequest& req, ISocketStream *stream) {
        auto it = callback_map_.find(req.url_path());
        if (it == callback_map_.end()) {
            return -1;
        }

        auto& callback = it->second;
        if (!check_callback(callback)) {
            return -1;
        }

        // request auth
        if (callback.OnAuth != nullptr && !callback.OnAuth(req)) {
            return -1;
        }

        Buffer buff;
        handshake(buff, req.websocket_key());
        // reply handshake response
        auto n = stream->send(&buff);
        if (n <= 0) return -1;


        // bad weak ptr when not make shared to WebSocketConnection
        auto connection = std::make_shared<WebSocketConnection>
                (stream, callback, workers_.get());
        connection->process();

        return 1;
    }

private:
    static void handshake(Buffer& handshake, const std::string & key) {
        static auto ws_magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        auto sha1 = Crypto::sha1(key + ws_magic_string);
        handshake << "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
        handshake << "Upgrade: websocket\r\n";
        handshake << "Connection: Upgrade\r\n";
        handshake << "Sec-WebSocket-Accept: " << Crypto::Base64::encode(sha1) << "\r\n";
        handshake << "\r\n";
    }

    bool check_callback(WebSocketCallback& callback) {
        if (callback.OnAuth == nullptr) return false;
        if (callback.OnOpen == nullptr) return false;
        if (callback.OnClose == nullptr) return false;
        if (callback.OnError == nullptr) return false;
        if (callback.OnMessage == nullptr) return false;
        if (callback.OnPing == nullptr) return false;
        if (callback.OnPong == nullptr) return false;
        return true;
    }

private:
    std::unordered_map<std::string, WebSocketCallback> callback_map_{};
    std::unique_ptr<WorkerPool> workers_;
};


static bool is_big_endian(void) {
    const int n = 1;
    if (*(char*) &n) return false;
    return true;
}

#define	hton64(val) is_big_endian() ? val : arch_netbswap_64(val)
#define	ntoh64(val) hton64(val)
}}