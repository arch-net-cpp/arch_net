#pragma once

#include <memory>

#include "socket_stream.h"
#include "ssl_socket_stream.h"
#include "resolver.h"
#include "load_balance.h"
#include "socket_pool.h"
#include "utils/future/future.h"
#include "buffer.h"
#include "mux_session.h"

namespace arch_net {

enum class ClientType {
    TCP_CLIENT,
    UDP_CLIENT,
    UDS_CLIENT_OVER_TCP,
    UDS_CLIENT_OVER_UDP,
    UDS_CLIENT_OVER_IPC
};

enum class ConnectionType {
    Short,
    Pooled,
    Multiplexing
};

enum class ClientErrorCode { kSuccess = 0, kError, kUserCallbackException };

struct ClientOption {
    ClientType client_type{ClientType::TCP_CLIENT};
    ConnectionType connection_type{ConnectionType::Short};

    int connection_timeout{0};       // connect timeout, default to 0: no timeout
    int read_write_timeout{0};       // recv timeout, default to 0: no timeout
    int max_retry{0};                // max retry times, default to 0: no retry
    // tls context
    ssl::TLSContext* tls_ctx{nullptr};
    // resolver
    ResolveType resolve_type;
    // load balance
    LoadBalanceType load_balance_type{LoadBalanceType::Random};
};

struct CallOption {
    int connection_timeout{0};
    int read_write_timeout{0};
    int max_retry{0};
    int seed{0};                     // consistent hash seed, must set when use consistent loadBalance
    std::unique_ptr<EndPoint> end_point{nullptr};
};

class ApplicationClient;

class ClientConnection {
public:
    ClientConnection(SocketStreamPtr stream, bool owner_ship, ApplicationClient* client,
        ConsistentIOWorker* worker) : stream_(stream),
        owner_ship_(owner_ship), client_(client), attached_worker_(worker) {}
    ~ClientConnection() { if (owner_ship_) stream_->close(); delete stream_; }

    // send
    size_t send_message(Buffer* buffer, int n = -1);
    size_t send_message(const void *buf, size_t count);
    Future<ClientErrorCode, size_t> future_send_message(Buffer* buffer, int n = -1);
    Future<ClientErrorCode, size_t> future_send_message(const void *buf, size_t count);

    // recv
    size_t recv_message(Buffer* buffer, int n = -1);
    size_t recv_message(void *buf, size_t count);
    Future<ClientErrorCode, size_t> future_recv_message(Buffer* buffer, int n = -1);
    Future<ClientErrorCode, size_t> future_recv_message(void *buf, size_t count);

    void custom_call(std::function<void()> function);

    SocketStreamPtr get_stream() const { return stream_; }

    void close_connection() { stream_->close();}

private:
    SocketStreamPtr stream_;
    bool owner_ship_;
    ApplicationClient* client_;
    std::unique_ptr<ConsistentIOWorker> attached_worker_{};
};

class ApplicationClient : public TNonCopyable {
public:
    ApplicationClient() {}

    int init(const std::string& host, int port, ClientOption& option);
    // fixed server list
    int init(const std::vector<std::pair<std::string, int>>& hosts, ClientOption& option);

    int init(const std::string& service_name, ClientOption& option);

    ClientConnection* get_connection(const CallOption* callopt = nullptr);

    Future<ClientErrorCode, ClientConnection* > future_get_connection(const CallOption* callopt = nullptr);

private:
    int do_init(ClientOption& option);

private:
    std::unique_ptr<ISocketClient> inner_client_;
    std::unique_ptr<ResolverWithLB> resolver_;
    ClientOption option_;
    acl::fiber_mutex mutex_;
    std::vector<std::unique_ptr<ClientConnection>> connections_;
};

}