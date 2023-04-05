
#include "pbrpc_controller.h"
namespace arch_net { namespace robin {

void RobinPBrpcController::Reset() {

}

bool RobinPBrpcController::Failed() const {
    return failed_;
}

std::string RobinPBrpcController::ErrorText() const {
    return failed_reason_;
}

void RobinPBrpcController::StartCancel() {

}

void RobinPBrpcController::SetFailed(const std::string &reason) {
    failed_ = true;
    failed_reason_ = reason;
}

bool RobinPBrpcController::IsCanceled() const {
    return false;
}

void RobinPBrpcController::NotifyOnCancel(google::protobuf::Closure *callback) {

}

void RobinPBrpcController::SetStreaming() {
    use_streaming_ = true;
}

void RobinPBrpcController::SetRemoteUseStreaming() {
    remote_use_streaming_ = true;
}

bool RobinPBrpcController::RemoteUseStreaming() const {
    return remote_use_streaming_;
}

bool RobinPBrpcController::UseStreaming() const {
    return use_streaming_;
}

StreamingConnection *RobinPBrpcController::GetStreamingConnection() const {
    return streaming_connection_.get();
}

void RobinPBrpcController::SetStreamingConnection(StreamingConnection *streaming) {
    streaming_connection_.reset(streaming);
}

CompressType RobinPBrpcController::GetCompressType() const {
    return compress_type_;
}

void RobinPBrpcController::SetCompressType(CompressType compress_type) {
    compress_type_ = compress_type;
}

}}