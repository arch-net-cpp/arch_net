#pragma once
#include "iostream"
#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include "../common.h"
#include "../socket_stream.h"
#include "../buffer.h"
#include "pbrpc_codec.h"
#include "pbrpc_error.h"
#include "pbrpc_callback.h"
#include "pbrpc_controller.h"
#include "rpc_streaming.h"
#include "../application_client.h"

#define MAX_CONNECTION 200

namespace arch_net { namespace robin {

class ClientSideConnection {
public:
    ClientSideConnection(ClientConnection* connection) : connection_(connection) {}
    ~ClientSideConnection();

    int process(const google::protobuf::MethodDescriptor *method,
                RobinPBrpcController *controller,
                const google::protobuf::Message *request,
                google::protobuf::Message *response,
                google::protobuf::Closure *done);
    void custom_call(std::function<void()> function);

    StreamingConnection* start_streaming();

    void reset(ClientConnection* connection);

private:
    std::unique_ptr<ClientConnection> connection_;

    Buffer req_meta_buffer_{};
    Buffer req_data_buffer_{};
    Buffer req_compress_buffer_{};

    Buffer resp_meta_buffer_{};
    Buffer resp_data_buffer_{};
    Buffer resp_compress_buffer_{};
    PBCodec codec_{};
};

class RobinPBrpcChannel : public ::google::protobuf::RpcChannel {
public:
    RobinPBrpcChannel() : application_client_(new ApplicationClient){}

    int init(const std::string& host, int port, ClientOption& option);
    // fixed server list
    int init(const std::vector<std::pair<std::string, int>>& hosts, ClientOption& option);

    int init(const std::string& service_name, ClientOption& option);

    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;

    ClientSideConnection* get_connection();

    void release_connection(ClientSideConnection*);

private:
    std::unique_ptr<ApplicationClient> application_client_{};
    acl::fiber_tbox<ClientSideConnection> connection_pool_{true};
};

class RobinPBrpcParallelChannel : public ::google::protobuf::RpcChannel {
public:

    void addSubCall(std::function<void(google::protobuf::Closure *done)> call) {
        sub_calls_.emplace_back(std::move(call));
    }

    void CallMethod(const google::protobuf::MethodDescriptor *method, google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request, google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;

private:
    std::vector<std::function<void(google::protobuf::Closure *done)>> sub_calls_;
};

}}