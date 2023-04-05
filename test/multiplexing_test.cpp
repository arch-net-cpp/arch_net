
#include "gtest/gtest.h"
#include "../mux_session.h"
#include "../common.h"
#include "../http/websocket.h"

using namespace arch_net;

static int test_handler(arch_net::ISocketStream* stream) {
    LOG(INFO) << "new connection";

    while (1){
        char buf[1024];
        auto n = stream->recv(buf, 1024);
        if (n <= 0) {
            break;
        }
        LOG(INFO) << "server recv: "<< std::string(buf, n);
        n = stream->send(buf, n);
        if (n <= 0) {
            break;
        }
        LOG(INFO) << "server send: "<< n;
    }

    LOG(INFO) << "connection closed";
    return 0;
}

TEST(Test_mux, Test_session)
{

    IOWorkerPool server_pool(1);

    IOWorkerPool client_pool(1);
    server_pool.addTask([&]() {
       auto tcp_server = new_tcp_socket_server();
       auto mux_server = mux::MultiplexingSocketServer(tcp_server, true);

       mux_server.init("127.0.0.1", 18888);
       mux_server.set_handler(test_handler);
       mux_server.start(1);

    });

//    acl::fiber_tbox<int> p1;
//    p1.pop();

    client_pool.addTask([&]() {

        acl_fiber_sleep(2);

        auto client = new_tcp_socket_client();

        {
            auto mux_client = std::make_unique<mux::MultiplexingSocketClient>(client, true);

            {
                auto stream = mux_client->connect("127.0.0.1", 18888);
                Buffer buffer;
                buffer.Append("1234567890");
                while (1) {
                    auto n = stream->send(&buffer);
                    LOG(INFO) << "client send: " << n;
                    if (n < 0) {
                        break;
                    }

                    n = stream->recv(&buffer);
                    if (n <= 0) {
                        break;
                    }
                    LOG(INFO) << "client recv: " << n << " " << buffer.ToString();
                    acl_fiber_sleep(1);
                }


                delete stream;
            }
        }
        LOG(INFO) << "client exit";

    });

    acl::fiber_tbox<int> p;
    p.pop();
}
