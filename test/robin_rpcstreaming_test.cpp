
#include "gtest/gtest.h"

#include "glog/logging.h"
#include "../socket_stream.h"
#include "../rpc/rpc_streaming.h"
#include "../mux_session.h"

#include "echo.pb.h"


using namespace arch_net;
using namespace arch_net::robin;

static int test_websocket_handler(arch_net::ISocketStream* stream) {

    IOWorkerPool pool(1);
    auto connection = std::make_shared<StreamingConnection>(stream, true);
    go[&, connection = connection]() {
        connection->process();
    };

    acl_fiber_sleep(1);
    ::example::EchoResponse response;
    response.set_message("123456");
    while (1) {
        auto ret = connection->send(&response);
        acl_fiber_sleep(1);
    }
    return 0;
}

TEST(Test_Robin, Test_Rpcstreaming_With_Mux)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    IOWorkerPool server_pool(1);
    server_pool.addTask([&]() {
        auto tcp_server = new_tcp_socket_server();
        auto mux_server = new arch_net::mux::MultiplexingSocketServer(tcp_server, true);

        mux_server->init("127.0.0.1", 18888);
        mux_server->set_handler(test_websocket_handler);
        mux_server->start(1);
    });
    acl::fiber_tbox<bool> box;
    box.pop();
}

TEST(Test_Robin, Test_Rpcstreaming_With_Mux_Client)
{

    IOWorkerPool client_pool(1);

    client_pool.addTask([&](){

        auto client = new_tcp_socket_client();
        auto mux_client = std::make_unique<arch_net::mux::MultiplexingSocketClient>(client, true);

        auto stream = mux_client->connect("127.0.0.1", 18888);

        auto connection = std::make_shared<StreamingConnection>(stream, false);

        go[&](){
            connection->process();
        };

        example::EchoResponse response;
        while (1){
            auto ret = connection->recv(&response);
            LOG(INFO) << "client: "<< response.DebugString();
        }

        delete stream;

        LOG(INFO) << "client exit";
    });
    acl::fiber_tbox<bool> box;
    box.pop();
}