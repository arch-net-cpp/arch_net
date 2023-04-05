
#include <gtest/gtest.h>
#include <glog/logging.h>
#include "../socket_stream.h"
#include "../ssl_socket_stream.h"

static int ssl_test_handler(arch_net::ISocketStream* stream) {
    LOG(INFO) << "new connection";

    while (true) {
        char buf[1024];
        auto size = stream->recv(buf, 1024);
        if (size <= 0) {
            break;
        }

        LOG(INFO) << std::string(buf, size);

        size = stream->send(buf, size);
        if (size <= 0) {
            break;
        }
        LOG(INFO) << "reply data";

        acl_fiber_sleep(1);
    }
    LOG(INFO) << "connection closed";

    return 0;

}

TEST(WEWD, WDW)
{

}
TEST(SSL_SOCKET_STREAM, Test_OPENSSLSOCKET)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    std::thread server_thread([](){
        auto ctx = arch_net::ssl::new_server_tls_context(
                "../../arch_net/test/mtls_test/keys/ca/ca_cert.pem",
                "../../arch_net/test/mtls_test/keys/server/server_cert.pem",
                "../../arch_net/test/mtls_test/keys/server/private/server_key.pem");

        if (!ctx) {
            return;
        }

        auto tls_server = arch_net::ssl::new_tls_server(ctx);
        tls_server->init("127.0.0.1", 8888);
        tls_server->set_handler([](arch_net::ISocketStream* stream) -> int{
            return ssl_test_handler(stream);
        });
        std::cout << "server start" << std::endl;
        tls_server->start();
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::thread client_thread([](){

        go[]() {

            auto ctx = arch_net::ssl::new_client_tls_context("../../arch_net/test/mtls_test/keys/ca/ca_cert.pem",
                                                             "../../arch_net/test/mtls_test/keys/client/client_cert.pem",
                                                             "../../arch_net/test/mtls_test/keys/client/private/client_key.pem");
            if (!ctx) {
                return;
            }
            auto client = arch_net::ssl::new_tls_client(ctx,  arch_net::new_tcp_socket_client(), true);
            auto stream = client->connect("127.0.0.1", 8888);
            std::string str = "test_client";
            if (!stream) {
                return;
            }
            for (int i = 0; i < 10; i++) {

                std::string str = "test_client" + std::to_string(i);

                int n = stream->send(str.c_str(), str.size());
                if (n <= 0) {
                    break;
                }

                char buf[1024] = {0};
                n = stream->recv(buf, 1024);
                if (n <= 0) {
                    break;
                }
                LOG(INFO) << "client recv message: " << std::string(buf);
                acl_fiber_sleep(1);
            }
        };

        acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "client exit" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::seconds(10000));
}

