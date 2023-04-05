
#include <gtest/gtest.h>
#include <glog/logging.h>
#include "../socket_stream.h"
#include "../buffer.h"
#include "../http/request.h"
#include "../socket.h"
using namespace arch_net::coin;
//static int test_handler(arch_net::ISocketStream* stream) {
//    LOG(INFO) << "new connection";
//    arch_net::Buffer buffer;
//    HttpRequest request;
//
//    while (true) {
//
//        request.reset();
//        auto size = request.recv_and_parse(stream);
//        if (size != OK) {
//            break;
//        }
//        buffer.Append("1234567890");
//        size = stream->send(buffer.data(), buffer.size());
//        if (size <= 0) {
//            break;
//        }
//        buffer.Retrieve(size);
//        LOG(INFO) << "reply data";
//    }
//    LOG(INFO) << "connection closed";
//
//    acl_fiber_sleep(1);
//    return 0;
//}
//
//TEST(TCP_SOCK, Test_tcp)
//{
//
//}
//
//TEST(SOCKET_STREAM, Test_TCPSOCKET)
//{
//    google::InitGoogleLogging("test");
//    FLAGS_logtostderr = true;
//
//    std::thread server_thread([](){
//        auto server = arch_net::new_tcp_socket_server();
//        server->init("127.0.0.1", 18888);
//        server->set_handler([](arch_net::ISocketStream* stream) -> int{
//            return test_handler(stream);
//        });
//        LOG(INFO) << "server start";
//        server->start();
//    });
//    std::this_thread::sleep_for(std::chrono::seconds(2000));
//
//    std::thread client_thread([](){
//
//        auto client = arch_net::new_tcp_socket_client();
//        auto stream = client->connect("127.0.0.1", 18888);
//        std::string str = "test_client";
//
//        stream->send(str.c_str(), str.size());
//
//        char buf[1024];
//        stream->recv(buf, 1024);
//
//        LOG(INFO) << "client recv message: " << std::string(buf);
//    });
//
//    std::this_thread::sleep_for(std::chrono::seconds(5));
//}

TEST(TestSocket, test_dns)
{
    std::vector<std::string> addrs;
    arch_net::dns_resolve("2408:8606:1800:501::2:f", addrs);

    std::cout << container_to_string(addrs.begin(), addrs.end()) << std::endl;
}