
#include "robin_pbrpc.h"
namespace arch_net { namespace robin {

void RobinPBrpcConnection::recv_and_parse(RequestInfo& req_info) {
    // recv meta
    req_info.meta_buff.Reset();
    if (req_info.meta_buff.ReadNFromSocketStream(stream_, 4) <= 0) {
        req_info.error_code = RpcRecvRemoteErr;
        return;
    }
    uint32_t header_size = req_info.meta_buff.ReadUInt32();

    if (header_size == 0) {
        // wo got a heartbeat request
        req_info.error_code = RpcHeartBeat;
        return;
    }
    if (req_info.meta_buff.ReadNFromSocketStream(stream_, header_size) <= 0) {
        req_info.error_code = RpcRecvRemoteErr;
        return;
    }
    // decode meta
    if (!codec_.DecodeRequestMeta(&req_info.meta_buff, req_info.req_meta) ) {
        req_info.error_code = RequestMetaDecodeErr;
        return;
    }

    // check service name & method name
    auto it = services_->find(req_info.req_meta.service_name);
    if (it == services_->end()) {
        req_info.error_code = ServiceNameNotFound;
        return;
    }

    auto service = it->second.service;
    auto md_it = it->second.mds.find(req_info.req_meta.method_name);
    if (md_it == it->second.mds.end()) {
        req_info.error_code = MethodNameNotFound;
        return;
    }

    // recv data
    req_info.data_buff.Reset();
    if (req_info.data_buff.ReadNFromSocketStream(stream_, req_info.req_meta.data_size) <= 0 ) {
        req_info.error_code = ReadRequestDataErr;
        return;
    }
    auto recv_msg = service->GetRequestPrototype(md_it->second).New();
    auto resp_msg = service->GetResponsePrototype(md_it->second).New();

    if (!codec_.DecodeRequestData(req_info.req_meta, recv_msg, &req_info.data_buff, &req_info.compress_buffer)) {
        req_info.error_code = ParseRequestDataErr;
        return;
    }

    req_info.error_code = RpcSuccess;
    req_info.service = service;
    req_info.md = md_it->second;
    req_info.recv_msg = recv_msg;
    req_info.resp_msg = resp_msg;
}

void RobinPBrpcConnection::handle_and_send(RequestInfo& req_info, ResponseInfo& resp_info) {
    auto* controller = new RobinPBrpcController();
    controller->SetCompressType((CompressType)req_info.req_meta.compress_type);
    if (req_info.req_meta.streaming_type) {
        controller->SetRemoteUseStreaming();
    }

    resp_info.controller = controller;
    auto done = robin::NewCallback(
            this,
            &RobinPBrpcConnection::on_resp_msg_filled,
            &req_info,
            &resp_info);

    req_info.service->CallMethod(req_info.md, controller, req_info.recv_msg, req_info.resp_msg, done);

    channel_.pop();
}

void RobinPBrpcConnection::on_resp_msg_filled(RequestInfo* req_info, ResponseInfo* resp_info) {
    auto &recv_msg_guard = req_info->recv_msg;
    auto &resp_msg_guard = req_info->resp_msg;
    auto &controller_guard = resp_info->controller;

    bool encoded = false;
    if (controller_guard->Failed()) {
        encoded = codec_.EncodeRPCErrorResponse(RPCError::BusinessError,
                                                controller_guard->ErrorText(), &resp_info->meta_buff);
    } else {
        encoded = codec_.EncodeRPCResponse(RPCError::RpcSuccess,
                                           controller_guard->ErrorText(),
                                           controller_guard->GetCompressType(),
                                           controller_guard->UseStreaming() ? 1 : 0,
                                           resp_msg_guard, &resp_info->meta_buff,
                                           &resp_info->data_buff, &resp_info->compress_buffer);
    }
    if (!encoded) {
        resp_info->error_code = -1;
        channel_.push(nullptr);
        return;
    }

    auto ret = stream_->send(&resp_info->meta_buff);
    if (ret <= 0) {
        resp_info->error_code = -1;
        channel_.push(nullptr);
    }

    if (controller_guard->Failed()) {
        resp_info->error_code = -1;
        channel_.push(nullptr);
    }
    ret = stream_->send(controller_guard->UseCompression() ? &resp_info->compress_buffer : &resp_info->data_buff);
    if (ret <= 0) {
        resp_info->error_code = -1;
        channel_.push(nullptr);
    }

    resp_info->error_code = 0;
    channel_.push(nullptr);

    // streaming
    if (controller_guard->RemoteUseStreaming() && controller_guard->UseStreaming()) {
        auto streaming = new StreamingConnection(stream_, true);
        controller_guard->SetStreamingConnection(streaming);
        (void )create_fiber([&, streaming=streaming](){
            streaming->process();
            return 0;
        });
    }
}

}}