#pragma once
#include "common.h"
#include <gflags/gflags.h>

#define HeaderSize 12
#define ProtoMagicValue 0


namespace arch_net { namespace mux {

static int32_t FLAGS_MaxStreamWindowSize = 256 * 1024;
static int32_t FLAGS_AcceptBacklog = 1024;
static bool FLAGS_EnableKeepAlive = true;
static int32_t FLAGS_KeepAliveInterval = 30 * 1000;
static int32_t FLAGS_ConnectionWriteTimeout = 30 * 1000;
static int32_t FLAGS_StreamCloseTimeout = 30 * 1000;
static int32_t FLAGS_StreamOpenTimeout = 30 * 1000;

typedef std::function<int(Buffer*)> OnReceivedCallback;

static int SendError = -1;
static int SendSuccess = 1;

static int RecvSuccess = 1;
static int RecvError = -1;

enum MuxStreamType{
    Client,
    Server
};

enum Flags {
    // SYN is sent to signal a new stream. May
    // be sent with a data payload
    flagSYN = 1,
    // ACK is sent to acknowledge a new stream. May
    // be sent with a data payload
    flagACK = 2,
    // FIN is sent to half-close the given stream.
    // May be sent with a data payload.
    flagFIN = 4,
    // RST is used to hard close a given stream.
    flagRST = 8,
};

enum MsgType {
    // Data is used for data frames. They are followed
    // by length bytes worth of payload.
    typeData = 0,

    // WindowUpdate is used to change the window of
    // a given stream. The length indicates the delta
    // update to the window.
    typeWindowUpdate = 1,

    // Ping is sent as a keep-alive or to measure
    // the RTT. The StreamID and Length value are echoed
    // back in the response.
    typePing = 2,

    // GoAway is sent to terminate a session. The StreamID
    // should be 0 and the length is an error code.
    typeGoAway = 3,
};

enum GoAwayType {
    // goAwayNormal is sent on a normal termination
    goAwayNormal = 0,

    // goAwayProtoErr sent on a protocol error
    goAwayProtoErr = 1,

    // goAwayInternalErr sent on an internal error
    goAwayInternalErr = 2,
};

enum StreamState {
    StreamInit,
    StreamSYNSent,
    StreamSYNReceived,
    StreamEstablished,
    StreamLocalClose,
    StreamRemoteClose,
    StreamClosed,
    StreamReset,
};

class Header {
public:
    Header() {}

    uint8_t msg_type() { return msg_type_;}

    uint16_t flags() { return flags_;}

    uint32_t stream_id() { return stream_id_;}

    uint32_t length() { return length_;}

    std::string to_string() { return string_printf("type: %d, flags: %d, st_id: %d, length: %d",
                                                   msg_type_, flags_, stream_id_, length_);}

    void encode(Buffer* buff, uint8_t msg_type, uint16_t flags, uint32_t stream_id, uint32_t length) {
        buff->Reset();
        buff->AppendUInt8(ProtoMagicValue);
        buff->AppendUInt8(msg_type);
        buff->AppendUInt16(flags);
        buff->AppendUInt32(stream_id);
        buff->AppendUInt32(length);
    }

    int recv_and_decode(Buffer* buff, ISocketStream* stream) {
        buff->Reset();
        auto n = buff->ReadNFromSocketStream(stream, HeaderSize);
        if (n <= 0) {
            return -1;
        }
        auto mag_val = buff->ReadUInt8();
        if (mag_val != ProtoMagicValue) {
            return -1;
        }
        msg_type_ = buff->ReadUInt8();
        if (msg_type_ < typeData || msg_type_ > typeGoAway) {
            return -2;
        }
        flags_ = buff->ReadUInt16();
        stream_id_ = buff->ReadUInt32();
        length_ = buff->ReadUInt32();

        return 1;
    }
private:
    uint8_t msg_type_;
    uint16_t flags_;
    uint32_t stream_id_;
    uint32_t length_;
};

}}