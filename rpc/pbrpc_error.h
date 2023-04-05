#pragma once
#include "iostream"

namespace arch_net { namespace robin {

enum RPCError {
    RequestMetaDecodeErr = -8001,
    RequestMetaEncodeErr = -8002,

    ServiceNameNotFound  = -8003,
    MethodNameNotFound   = -8004,
    ReadRequestDataErr   = -8005,
    ParseRequestDataErr  = -8006,
    ParseResponseDataErr = -8007,

    SendRequestTimeout   = -8100,
    SendRequestRemoteClose = -8101,


    BusinessError        = -8100,

    RpcTimeOut           = -8200,

    RpcRecvRemoteErr     = -8300,

    RpcSuccess           = 1,
    RpcHeartBeat         = 2,
};

static std::unordered_map<int, std::string> ErrCode2Msg = {
        {RequestMetaDecodeErr, "Request Meta Decode Error"},
        {ServiceNameNotFound, "Service Name Not Found Error"},
        {MethodNameNotFound, "Method Name Not Found Error"},
        {ReadRequestDataErr, "Read Request Data Error"},
};

}}