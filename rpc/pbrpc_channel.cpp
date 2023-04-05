
#include "pbrpc_channel.h"

namespace arch_net { namespace robin {

ClientSideConnection::~ClientSideConnection() {}

void ClientSideConnection::reset(ClientConnection *connection) {
    connection_.reset(connection);
    req_meta_buffer_.Reset();
    req_data_buffer_.Reset();
    req_compress_buffer_.Reset();

    resp_meta_buffer_.Reset();
    resp_data_buffer_.Reset();
    resp_compress_buffer_.Reset();
}

int ClientSideConnection::process(const google::protobuf::MethodDescriptor *method,
        RobinPBrpcController *controller,const google::protobuf::Message *request,
        google::protobuf::Message *response, google::protobuf::Closure *done) {

    (void)done;

    bool encode_err =
    codec_.EncodeRPCRequest(method, controller->GetCompressType(),
                    controller->UseStreaming() ? 1 : 0, request,
                    &req_meta_buffer_, &req_data_buffer_, &req_compress_buffer_);
    if (!encode_err) {
        controller->SetFailed("request encode error");
        return RequestMetaEncodeErr;
    }

    auto ret = connection_->send_message(&req_meta_buffer_);
    if (ret <= 0) {
        controller->SetFailed("request meta send error");
        if (ret <= SendTimeout) {
            return SendRequestTimeout;
        }
        return SendRequestRemoteClose;
    }

    ret = connection_->send_message(controller->UseCompression() ? &req_compress_buffer_ : &req_data_buffer_);
    if (ret <= 0) {
        controller->SetFailed("request message send error");
        if (ret <= SendTimeout) {
            return SendRequestTimeout;
        }
        return SendRequestRemoteClose;
    }

    // wait response
    uint32_t header_size = 0;
    while (true) {
        resp_meta_buffer_.Reset();
        if (connection_->recv_message(&resp_meta_buffer_, 4) <= 0) {
            return RpcRecvRemoteErr;
        }
        header_size = resp_meta_buffer_.ReadUInt32();
        if (header_size > 0) {
            break;
        }
    }
    if (connection_->recv_message(&resp_meta_buffer_, header_size) <= 0) {
        return RpcRecvRemoteErr;
    }
    //TODO
    PBRpcRespMeta resp_meta;
    codec_.DecodeResponseMeta(&resp_meta_buffer_, resp_meta);
    if (resp_meta.status_code != RPCError::RpcSuccess) {
        controller->SetFailed(resp_meta.error_msg);
        return resp_meta.status_code;
    }

    resp_data_buffer_.Reset();
    if (connection_->recv_message(&resp_data_buffer_, resp_meta.data_size) <= 0) {
        controller->SetFailed("");
        return RpcRecvRemoteErr;
    }

    if (!codec_.DecodeResponseData(resp_meta, response, &resp_data_buffer_, &resp_compress_buffer_)) {
        controller->SetFailed("");
        return ParseResponseDataErr;
    }

    if (resp_meta.streaming_type) {
        controller->SetRemoteUseStreaming();
    }

    return RpcSuccess;
}

void ClientSideConnection::custom_call(std::function<void()> function) {
    connection_->custom_call(std::move(function));
}

StreamingConnection* ClientSideConnection::start_streaming() {
    auto streaming = new StreamingConnection(connection_->get_stream(), false);
    connection_->custom_call([&, streaming=streaming]() mutable {
        streaming->process();
    });

    return streaming;
}

int RobinPBrpcChannel::init(const std::string &service_name, ClientOption &option) {
    return application_client_->init(service_name, option);
}

int RobinPBrpcChannel::init(const std::string &host, int port, ClientOption &option) {
    return application_client_->init(host, port, option);
}

int RobinPBrpcChannel::init(const std::vector<std::pair<std::string, int>> &hosts, ClientOption &option) {
    return application_client_->init(hosts, option);
}

ClientSideConnection *RobinPBrpcChannel::get_connection() {
    auto underlying_conn = application_client_->get_connection();
    if (!underlying_conn) {
        return nullptr;
    }
    auto connection = connection_pool_.pop(0);
    if (!connection) {
        return new ClientSideConnection(underlying_conn);
    }
    connection->reset(underlying_conn);
    return connection;
}

void RobinPBrpcChannel::release_connection(ClientSideConnection * connection) {
    if (connection_pool_.size() >= MAX_CONNECTION) {
        delete connection;
        return;
    }
    connection_pool_.push(connection);
}

void RobinPBrpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                   google::protobuf::RpcController *controller,
                                   const google::protobuf::Message *request,
                                   google::protobuf::Message *response, google::protobuf::Closure *done) {
    auto conn = this->get_connection();
    if (!conn) {
        controller->SetFailed("invalid connection");
        if (done) {
            done->Run();
        }
        return;
    }

    acl::fiber_tbox<bool> chn_;
    conn->custom_call([&, conn=conn, method=method, c=controller, request=request,
                       response=response, done=done]() {

        auto controller = static_cast<RobinPBrpcController*>(c);
        auto ret = conn->process(method, controller, request, response, done);
        if (ret == RpcTimeOut) {

        }
        if (done) {
            done->Run();
        }

        if (controller->UseStreaming() && controller->RemoteUseStreaming()) {
            auto streaming = conn->start_streaming();
            controller->SetStreamingConnection(streaming);
        } else {
            this->release_connection(conn);
        }

        if (!done) {
            chn_.push(nullptr);
        }
    });

    if (!done) {
        chn_.pop();
    }
}

static void sub_call_done(acl::wait_group* wg) {
    assert(wg);
    wg->done();
}

void RobinPBrpcParallelChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                                           google::protobuf::RpcController *controller,
                                           const google::protobuf::Message *request,
                                           google::protobuf::Message *response,
                                           google::protobuf::Closure *done) {
    (void)method;
    (void)controller;
    (void)request;
    (void)response;
    (void)done;

    acl::wait_group wg;
    wg.add(sub_calls_.size());
    auto new_done = robin::NewCallback(sub_call_done, &wg);

    for (const auto & sub_call : sub_calls_) {
        sub_call(new_done);
    }
    wg.wait();

    sub_calls_.clear();
}


}}