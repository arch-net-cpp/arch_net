#include "application_client.h"

namespace arch_net {

int ApplicationClient::init(const std::string &host, int port, ClientOption& option) {
    return init({{host, port}}, option);
}

int ApplicationClient::init(const std::vector<std::pair<std::string, int>> &hosts, ClientOption &option) {
    resolver_ = std::make_unique<FixedListResolver>(hosts);
    resolver_->with_load_balance(new_load_balance(option.load_balance_type));
    return do_init(option);
}

int ApplicationClient::init(const std::string &service_name, ClientOption &option) {
    resolver_ = std::make_unique<DNSResolver>(service_name);
    resolver_->with_load_balance(new_load_balance(option.load_balance_type));
    return do_init(option);
}

int ApplicationClient::do_init(ClientOption &option) {

    int val;
    option_ = option;

    // only support tcp now
    (void )option.client_type;
    auto client = new_tcp_socket_client();
    if (!client) {
        //log
        return -1;
    }
    if (option.tls_ctx != nullptr) {
        client = ssl::new_tls_client(option.tls_ctx, client, true);
        if (!client) {
            //log
            return -1;
        }
    }
    switch (option.connection_type) {
        case ConnectionType::Short:
            inner_client_.reset(client);
            break;
        case ConnectionType::Pooled:
            inner_client_ = std::make_unique<TcpSocketPoolClient>(client, true);
            break;
        case ConnectionType::Multiplexing:
            inner_client_ = std::make_unique<mux::MultiplexingSocketClient>(client, true);
            break;
    }
    if (!inner_client_) {
        return -1;
    }
    return 1;
}

size_t ClientConnection::send_message(Buffer *buffer, int n) {
    auto f = future_send_message(buffer, n);
    size_t val;
    auto ret = f.get(val);
    (void )ret;
    return val;
}

size_t ClientConnection::send_message(const void *buf, size_t count) {
    auto f = future_send_message(buf, count);
    size_t val;
    auto ret = f.get(val);
    (void )ret;
    return val;
}

size_t ClientConnection::recv_message(Buffer *buffer, int n) {
    auto f = future_recv_message(buffer, n);
    size_t val;
    auto ret = f.get(val);
    (void )ret;
    return val;
}

size_t ClientConnection::recv_message(void *buf, size_t count) {
    auto f = future_recv_message(buf, count);
    size_t val;
    auto ret = f.get(val);
    (void )ret;
    return val;
}

void ClientConnection::custom_call(std::function<void()> function) {
    attached_worker_->addTask(std::move(function));
}

Future<ClientErrorCode, size_t> ClientConnection::future_send_message(Buffer *buffer, int n) {
    if (n < 0) {
        return future_send_message(buffer->data(), buffer->size());
    }
    return future_send_message(buffer->data(), n);
}

Future<ClientErrorCode, size_t> ClientConnection::future_send_message(const void *buf, size_t count) {
    Promise<ClientErrorCode, size_t> promise;
    attached_worker_->addTask([buf = buf, promise, count, this]() {
        auto ret = stream_->send(buf, count);
        promise.setValue(ClientErrorCode::kSuccess, ret);
    });
    return promise.getFuture();
}

Future<ClientErrorCode, size_t> ClientConnection::future_recv_message(Buffer *buffer, int n) {
    Promise<ClientErrorCode, size_t> promise;
    attached_worker_->addTask([buffer = buffer, promise, n, this]() {
        if (n < 0) {
            auto ret = buffer->ReadFromSocketStream(stream_);
            promise.setValue(ClientErrorCode::kSuccess, ret);
            return ;
        }
        auto ret = buffer->ReadNFromSocketStream(stream_, n);
        promise.setValue(ClientErrorCode::kSuccess, ret);
    });
    return promise.getFuture();
}

Future<ClientErrorCode, size_t> ClientConnection::future_recv_message(void *buf, size_t len) {
    Promise<ClientErrorCode, size_t> promise;
    attached_worker_->addTask([buffer = (char*)buf, promise, len, this]() {
        uint32_t sent = 0;
        while (sent < len) {
            uint32_t b = stream_->recv(buffer + sent, len - sent);
            if (b <= 0) {
                promise.setValue(ClientErrorCode::kError, b);
                return;
            }
            sent += b;
        }
        promise.setValue(ClientErrorCode::kSuccess, sent);
    });
    return promise.getFuture();
}

ClientConnection* ApplicationClient::get_connection(const CallOption *callopt) {
    auto f = future_get_connection(callopt);
    ClientConnection* conn;
    auto ret = f.get(conn);
    if (ret != ClientErrorCode::kSuccess) {
        return nullptr;
    }
    return conn;
}

// calloption priority: self-defined > remote config > client config
Future<ClientErrorCode, ClientConnection*> ApplicationClient::future_get_connection(const CallOption *callopt) {

    Promise<ClientErrorCode, ClientConnection*> promise;
    auto attached_worker = new ConsistentIOWorker();
    attached_worker->addTask([callopt, promise, attached_worker, this]() {
        SocketStreamPtr stream;
        if (callopt && callopt->end_point) {
            stream = inner_client_->connect(*(callopt->end_point));
            if (!stream) {
                promise.setValue(ClientErrorCode::kError, nullptr);
                return;
            }
        } else {
            int seed = 0;
            if (callopt) {
                seed = callopt->seed;
            }
            mutex_.lock();
            auto node = resolver_->get_next(seed);
            mutex_.unlock();
            stream = inner_client_->connect(node.endpoint);
            if (!stream) {
                promise.setValue(ClientErrorCode::kError, nullptr);
                return;
            }
        }
        promise.setValue(ClientErrorCode::kSuccess, new ClientConnection(stream, true, this, attached_worker));
    });

    return promise.getFuture();
}

}