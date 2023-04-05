#pragma once
#include "common.h"
#include "socket_stream.h"
#include "ssl_socket_stream.h"
#include "mux_session.h"
#include <gflags/gflags.h>

#include <memory>

namespace arch_net {

enum ServerType {
    TCP_SERVER,
//    UDP_SERVER,
//    UDS_OVER_TCP,
//    UDS_OVER_UDP,
//    UDS_OVER_IPC
};

enum class ServerConnectionType {
    Normal,
    Multiplexing,
};

struct ServerConfig {
    std::string ip_addr{"0.0.0.0"};
    int port{7890};
    ServerType type{TCP_SERVER};
    ServerConnectionType connection_type{ServerConnectionType::Normal};
    std::string uds_path{};
    std::string tls_ca_path{};
    std::string tls_cert_path{};
    std::string tls_key_path{};
    int io_thread_num{1};
};


class ApplicationServer : public TNonCopyable {
public:

    ApplicationServer() {}

    int listen_and_serve(ServerConfig* config) {
        if (!validate(config)) {
            throw "invalidate config";
        }
        // only support tcp now
        auto server = new_tcp_socket_server();
        switch (config->connection_type) {
            case ServerConnectionType::Normal:
                inner_server_.reset(server);
                break;
            case ServerConnectionType::Multiplexing:
                inner_server_ = std::make_unique<mux::MultiplexingSocketServer>(server, true);
                break;
        }
        return start(config);
    }

    int listen_and_serve_tls(ServerConfig* config) {
        if (!validate(config)) {
            throw "invalidate config";
        }

        auto ctx = ssl::new_server_tls_context(config->tls_ca_path,
                                               config->tls_cert_path,
                                               config->tls_key_path);
        if (!ctx) {
            return ERR;
        }
        auto server = ssl::new_tls_server(ctx);
        switch (config->connection_type) {
            case ServerConnectionType::Normal:
                inner_server_.reset(server);
                break;
            case ServerConnectionType::Multiplexing:
                inner_server_ = std::make_unique<mux::MultiplexingSocketServer>(server, true);
                break;
        }
        return start(config);
    }

protected:
    virtual int handle_connection(ISocketStream* stream) = 0;

private:
    bool validate(ServerConfig* config) {
        return true;
    }

    int start(ServerConfig* config) {
        int ret = inner_server_->init(config->ip_addr, config->port);
        if (ret < 0) {
            return ERR;
        }
        inner_server_->set_handler([this](ISocketStream* stream)->int {
            return this->handle_connection(stream);
        });

        inner_server_->start(config->io_thread_num);
        return OK;
    }

private:
    std::unique_ptr<ISocketServer> inner_server_;
};


}