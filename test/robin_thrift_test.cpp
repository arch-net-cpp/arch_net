#include <gtest/gtest.h>

#include "Twitter.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include "../rpc/robin_thrift.h"
#include "../rpc/thrift_channel.h"

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::example;

DEFINE_int32(io_thread_num, 1, "IO thread num of application server");
DEFINE_string(listen_addr, "0.0.0.0", "application server listen addr");
DEFINE_string(uds_path, "", "application server listen unix socket path");
DEFINE_int32(listen_port, 18888, "application server listen port");
DEFINE_string(tls_ca_path, "./keys/ca/ca_cert.pem", "ca_cert.pem path");
DEFINE_string(tls_cert_path, "./keys/server/server_cert.pem", "server_cert.pem path");
DEFINE_string(tls_key_path, "./keys/server/private/server_key.pem", "server_key.pem path");

class TwitterHandler : virtual public TwitterIf {
public:
    TwitterHandler() {
        // Your initialization goes here
    }

    void sendString(std::string& _return, const std::string& data) {
        // Your implementation goes here
        printf("sendString\n");
        uint64_t i = 0;
        _return = "server: " + data;
    }
};

TEST(ROBIN, Test_thrift)
{
    arch_net::ServerConfig config;
    config.ip_addr = "127.0.0.1";
    config.port = 18888;
    config.uds_path = FLAGS_uds_path;
    config.tls_ca_path = FLAGS_tls_ca_path;
    config.tls_cert_path = FLAGS_tls_cert_path;
    config.tls_key_path = FLAGS_tls_key_path;
    config.io_thread_num = FLAGS_io_thread_num;


    std::thread server_thread([&](){
        ::std::shared_ptr<TwitterHandler> handler(new TwitterHandler());
        ::std::shared_ptr<TProcessor> processor(new TwitterProcessor(handler));
        ::std::shared_ptr<TProtocolFactory> protocolFactory(new TCompactProtocolFactory());
        //::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

        arch_net::robin::RobinThriftServer server(processor, protocolFactory);
        server.listen_and_serve(&config);
    });


    std::this_thread::sleep_for(std::chrono::seconds(2));

    go[](){
        try {
            arch_net::robin::ThriftChannel channel;
            arch_net::ClientOption option;
            channel.init("127.0.0.1", 18888, option);

            TwitterClient client(channel.newCompactProtocol());
            while (true) {
                std::string data = "i am client 1";
                std::string recv;
                client.sendString(recv, data);

                std::cout << recv << std::endl;
                acl_fiber_sleep(1);
            }

        } catch (TException& tx) {
            std::cout << "ERROR: " << tx.what() << std::endl;
        }
    };
//
//    go[](){
//        acl_fiber_sleep(1);
//
//        try {
//
//            arch_net::robin::ThriftChannel channel;
//            arch_net::ClientOption option;
//            channel.init("127.0.0.1", 18888, option);
//
//            TwitterClient client(channel.newBinaryProtocol());
//
//            while (true) {
//                std::string data = "i am client 2";
//                std::string recv;
//                client.sendString(recv, data);
//                std::cout << recv << std::endl;
//                acl_fiber_sleep(1);
//            }
//
//        } catch (TException& tx) {
//            std::cout << "ERROR: " << tx.what() << std::endl;
//        }
//    };


    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}