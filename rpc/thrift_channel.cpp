
#include "thrift_channel.h"
namespace arch_net { namespace robin {

int ThriftChannel::init(const std::string &host, int port, ClientOption &option) {
    return application_client_->init(host, port, option);
}

int ThriftChannel::init(const std::string &service_name, ClientOption &option) {
    return application_client_->init(service_name, option);
}

int ThriftChannel::init(const std::vector<std::pair<std::string, int>> &hosts, ClientOption &option) {
    return application_client_->init(hosts, option);
}

std::shared_ptr<TProtocol> ThriftChannel::newBinaryProtocol() {
    auto underlying_conn = application_client_->get_connection();
    if (!underlying_conn) {
        return nullptr;
    }

    std::shared_ptr<TTransport> socket = std::make_shared<RTSocket>(underlying_conn);
    std::shared_ptr<TTransport> transport(new TFramedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    return protocol;
}

std::shared_ptr<TProtocol> ThriftChannel::newCompactProtocol() {
    auto underlying_conn = application_client_->get_connection();
    if (!underlying_conn) {
        return nullptr;
    }
    std::shared_ptr<TTransport> socket = std::make_shared<RTSocket>(underlying_conn);
    std::shared_ptr<TTransport> transport(new TFramedTransport(socket));
    std::shared_ptr<TProtocol> protocol(new TCompactProtocol(transport));
    return protocol;
}

}}
