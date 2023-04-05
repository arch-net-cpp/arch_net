#pragma once
#include "../common.h"
#include "../socket_stream.h"
#include "../ssl_socket_stream.h"
#include "thrift_socket.h"
#include "../common.h"
#include "../application_client.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/protocol/TCompactProtocol.h>
#include <thrift/transport/TTransportUtils.h>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;


namespace arch_net { namespace robin {

class ThriftChannel : TNonCopyable{
public:
    ThriftChannel() : application_client_(new ApplicationClient) {}

    int init(const std::string& host, int port, ClientOption& option);
    // fixed server list
    int init(const std::vector<std::pair<std::string, int>>& hosts, ClientOption& option);

    int init(const std::string& service_name, ClientOption& option);

    std::shared_ptr<TProtocol> newBinaryProtocol();

    std::shared_ptr<TProtocol> newCompactProtocol();

public:
    std::unique_ptr<ApplicationClient> application_client_;
};

}}