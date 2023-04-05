#pragma once

#include "../application_server.hpp"
#include "pbrpc_codec.h"
#include "pbrpc_controller.h"
#include "pbrpc_callback.h"
#include "pbrpc_error.h"

namespace arch_net { namespace robin {

static const int PBRPC_HEARTBEAT_INTERVAL = 30 * 1000;

struct ServiceInfo {
    ::google::protobuf::Service* service{};
    const ::google::protobuf::ServiceDescriptor* sd{};
    std::map<std::string, const ::google::protobuf::MethodDescriptor*> mds;
};//ServiceInfo

struct RequestInfo {
    int error_code{};
    PBRpcReqMeta req_meta;
    google::protobuf::Service* service{};
    const google::protobuf::MethodDescriptor* md{};
    google::protobuf::Message *recv_msg{};
    google::protobuf::Message *resp_msg{};
    Buffer meta_buff{128};
    Buffer data_buff;
    Buffer compress_buffer;
    void clear() {
        error_code = 0, req_meta.clear(), service = nullptr, md = nullptr,
        recv_msg = nullptr, resp_msg = nullptr, meta_buff.Reset(),
        data_buff.Reset(), compress_buffer.Reset();}
};
struct ResponseInfo {
    int error_code{};
    RobinPBrpcController* controller{};
    PBRpcRespMeta resp_meta;
    Buffer meta_buff{128};
    Buffer data_buff;
    Buffer compress_buffer;
    void clear() {
        error_code = 0, controller = nullptr, resp_meta.clear(), meta_buff.Reset(),
        data_buff.Reset(), compress_buffer.Reset();
    }
};

class RobinPBrpcConnection {
public:
    RobinPBrpcConnection(ISocketStream *stream, std::unordered_map<std::string, ServiceInfo> *services)
        : stream_(stream), services_(services) {}

    void recv_and_parse(RequestInfo& req_info);

    void handle_and_send(RequestInfo& req_info, ResponseInfo& resp_info);

private:
    void on_resp_msg_filled(RequestInfo* req_info, ResponseInfo* resp_info);

private:
    ISocketStream *stream_{};
    std::unordered_map<std::string, ServiceInfo> *services_{};
    acl::fiber_tbox<int> channel_;

    PBCodec codec_;
};

class RobinPBrpcServer : public ApplicationServer {
public:
    RobinPBrpcServer() : ApplicationServer(), workers_(new IOWorkerPool()) {}

    void add_service(::google::protobuf::Service* service) {
        ServiceInfo service_info;
        service_info.service = service;
        service_info.sd = service->GetDescriptor();
        for (int i = 0; i < service_info.sd->method_count(); ++i) {
            service_info.mds[service_info.sd->method(i)->name()] = service_info.sd->method(i);
        }
        services_[service_info.sd->name()] = service_info;
    }

private:
    virtual int handle_connection(ISocketStream* stream) {
        RobinPBrpcConnection conn(stream,  &services_);
        RequestInfo* req_info = request_pool_.Get();
        ResponseInfo* resp_info = response_pool_.Get();
        req_info->clear();
        resp_info->clear();

        while (true) {
            conn.recv_and_parse(*req_info);
            if (req_info->error_code < 0) {
                return -1;
            }
            if (req_info->error_code == RpcHeartBeat) continue;
            conn.handle_and_send(*req_info, *resp_info);
            if (resp_info->error_code < 0) {
                return -1;
            }
        }
    }

private:
    //service_name -> {Service*, ServiceDescriptor*, MethodDescriptor* []}
    std::unordered_map<std::string, ServiceInfo> services_;
    std::unique_ptr<WorkerPool> workers_;

    ObjectPool<RequestInfo> request_pool_{};
    ObjectPool<ResponseInfo> response_pool_{};
};

}}