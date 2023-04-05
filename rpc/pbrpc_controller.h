#pragma once

#include <iostream>
#include "google/protobuf/service.h"
#include "google/protobuf/stubs/common.h"
#include "../common.h"
#include "rpc_streaming.h"

namespace arch_net { namespace robin {

class RobinPBrpcController : public ::google::protobuf::RpcController {
public:
    void Reset() override;

    bool Failed() const override;

    std::string ErrorText() const override;

    void StartCancel() override;

    void SetFailed(const std::string &reason) override;

    bool IsCanceled() const override;

    void NotifyOnCancel(google::protobuf::Closure *callback) override;

    void SetStreaming();

    void SetRemoteUseStreaming();

    bool UseStreaming() const;

    bool RemoteUseStreaming() const;

    void SetStreamingConnection(StreamingConnection* streaming);

    StreamingConnection* GetStreamingConnection() const;

    void SetCompressType(CompressType compress_type);

    CompressType GetCompressType() const;

    bool UseCompression() const { return compress_type_ != NO_COMPRESSION; }

private:
    bool failed_{false};
    std::string failed_reason_{};
    bool use_streaming_{false};
    bool remote_use_streaming_{false};
    std::unique_ptr<StreamingConnection> streaming_connection_{};
    CompressType compress_type_{NO_COMPRESSION};
};
}}