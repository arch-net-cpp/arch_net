#include <gtest/gtest.h>
#include "../http/trie.h"
#include "../http/router.h"
#include "../http/coin_server.h"
#include <glog/logging.h>
#include "../utils/monitor.h"
//#include "http_lib.h"

#include "../http/coin_client.h"

#include "../mux_session.h"

using namespace arch_net;
using namespace arch_net::coin;

DEFINE_int32(io_thread_num, 1, "IO thread num of application server");
DEFINE_string(listen_addr, "0.0.0.0", "application server listen addr");
DEFINE_string(uds_path, "", "application server listen unix socket path");
DEFINE_int32(listen_port, 18888, "application server listen port");
DEFINE_string(tls_ca_path, "./keys/ca/ca_cert.pem", "ca_cert.pem path");
DEFINE_string(tls_cert_path, "./keys/server/server_cert.pem", "server_cert.pem path");
DEFINE_string(tls_key_path, "./keys/server/private/server_key.pem", "server_key.pem path");

TEST(HTTP, Test_Trie)
{
    Trie trie;
    auto node1 = trie.add_node("/a/b/c");
    auto node2 = trie.add_node("/x/y/z");
    auto node3 = trie.add_node("/1/2/3");

    auto node4 = trie.add_node("/a/b/:id");


    Matched matched1;
    trie.match("/a/b/c", matched1);

    EXPECT_EQ(node1, matched1.node);

    Matched matched2;
    trie.match("/x/y/z", matched2);
    EXPECT_EQ(node2, matched2.node);

    Matched matched3;
    trie.match("/1/2/3", matched3);
    EXPECT_EQ(node3, matched3.node);

    Matched matched4;
    trie.match("/a/b/3", matched4);
    EXPECT_EQ(node4, matched4.node);
    EXPECT_EQ("3", matched4.params["id"]);

    std::vector<std::pair<std::string, std::string>> result;
    trie.walk_trie(result);

    int a = 0;
    a++;
    int b = a;
}

TEST(HTTP, Test_Router)
{
    Router router;

    router.use("/a", [](HttpRequest& req, HttpResponse& res, SideError& err){
        std::cout << "in /a"<< std::endl;
    });

    router.use("/a/b", [](HttpRequest& req, HttpResponse& res, SideError& err){
        std::cout << "in /a/b"<< std::endl;
        err.status_code = 500;
        err.error_msg = "internal error";
    });

    router.get("/a/b/c", [](HttpRequest& req, HttpResponse& res){
        std::cout << "in /a/b/c"<< std::endl;
        res.json(200, "{}");
    });

    router.get("/a/b/:id", [](HttpRequest& req, HttpResponse& res){
        std::cout << "in /a/b/:id"<< std::endl;
        std::cout << req.get_params("id")<< std::endl;
        res.json(200, "{}");
    });

    HttpRequest req;
    HttpResponse resp;

    arch_net::Buffer buffer;
    buffer.Append(
            "GET /a/b/1234/get?page=1 HTTP/1.1\r\n"
            "Host: 0.0.0.0=5000\r\n"
            "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
            "Accept-Language: en-us,en;q=0.5\r\n"
            "Accept-Encoding: gzip,deflate\r\n"
            "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
            "Keep-Alive: 300\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
    );
    req.parse(buffer);
    EXPECT_EQ(1, req.completed());
    EXPECT_EQ("GET", req.get_method());

    SideError err;
    router.handle(req, resp, err);

    std::vector<std::pair<std::string, std::string>> result;
    router.trie_.walk_trie(result);

    result.empty();
}

TEST(HTTP, Test_Server)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    arch_net::ServerConfig config;
    config.ip_addr = FLAGS_listen_addr;
    config.port = FLAGS_listen_port;
    config.uds_path = FLAGS_uds_path;
    config.tls_ca_path = FLAGS_tls_ca_path;
    config.tls_cert_path = FLAGS_tls_cert_path;
    config.tls_key_path = FLAGS_tls_key_path;
    config.io_thread_num = FLAGS_io_thread_num;

//    gflags::ParseCommandLineFlags(nullptr, nullptr, true);

    auto server = new_coin_server();
    defer(delete server);

    server->get("/a/b/c", [](HttpRequest& req, HttpResponse& res) {
        std::string data = R"({"data":"call coin server"})";
        res.json(200, data);
    });

    server->get("/a/b/:id/get", [](HttpRequest& req, HttpResponse& res) {
        auto id = req.get_params("id");
        std::string data;
        data.append(R"({"id":)").append(R"(")").append(id).append(R"("})");
        res.json(200, data);
    });


    server->listen_and_serve_tls(&config);
}

TEST(HTTP, Test_websocket)
{

    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    arch_net::ServerConfig config;
    config.ip_addr = FLAGS_listen_addr;
    config.port = FLAGS_listen_port;
    config.uds_path = FLAGS_uds_path;
    config.tls_ca_path = FLAGS_tls_ca_path;
    config.tls_cert_path = FLAGS_tls_cert_path;
    config.tls_key_path = FLAGS_tls_key_path;
    config.io_thread_num = FLAGS_io_thread_num;

    auto server = new_coin_server();
    defer(delete server);

    WebSocketCallback endpoint;
    endpoint.OnOpen = [&](WebSocketConnection*)->bool {
        return true;
    };
    endpoint.OnClose = [&](WebSocketConnection*)->bool {
        return true;
    };
    endpoint.OnPing = [&](WebSocketConnection*)->bool {
        return true;
    };
    endpoint.OnPong = [&](WebSocketConnection*)->bool {
        return true;
    };
    endpoint.OnError = [&](WebSocketConnection*)->bool {
        return true;
    };
    endpoint.OnMessage = [&](WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {

        std::cout << buff->ToString() << std::endl;
        uint64_t i = 0;
        while (i < UINT32_MAX/1000){
            i++;
        }
        conn->sendText(buff->ToString());
        return true;
    };
    server->websocket("/", endpoint);

    server->listen_and_serve(&config);
}

TEST(HTTP, Test_HttpClient)
{
    arch_net::coin::CoinClient client("http://127.0.0.1:8000");

    auto session = client.new_session();
    auto result = session->Get("/1.txt", {}, {}, nullptr);
    if (result.error() == Error::Success) {
        std::cout << result.value().body << std::endl;
    } else {
        std::cout << " error" << std::endl;
    }
}

TEST(HTTP, Test_HttpClient_WebSocket)
{
    arch_net::coin::CoinClient client("http://127.0.0.1:8080");

    auto session = client.new_session();

    WebSocketCallback callback;

    callback.OnOpen = [&](WebSocketConnection* conn){
        conn->sendText("hello world");
        return true;
    };

    callback.OnMessage = [&](WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {
        std::cout << buff->ToString() << std::endl;

        //conn->closeWebsocket();
        return true;
    };

    auto result = session->WebSocket("/ws", {}, {}, callback);
    if (result.error() == Error::Success) {
        std::cout << result.value().body << std::endl;
    } else {
        std::cout << " error" << std::endl;
    }
}

TEST(HTTPS, Test_HTTPS_SERVER_CLIENT)
{
    IOWorkerPool thread_pool;

    thread_pool.addTask([](){

        google::InitGoogleLogging("test");
        FLAGS_logtostderr = true;

        arch_net::ServerConfig config;
        config.ip_addr = "127.0.0.1";
        config.port = 8888;
        config.uds_path = "";
        config.tls_ca_path = "../../arch_net/test/mtls_test/keys/ca/ca_cert.pem";
        config.tls_cert_path = "../../arch_net/test/mtls_test/keys/server/server_cert.pem";
        config.tls_key_path = "../../arch_net/test/mtls_test/keys/server/private/server_key.pem";
        config.io_thread_num = FLAGS_io_thread_num;

        auto server = new_coin_server();
        defer(delete server);

        server->get("/a/b/c", [](HttpRequest& req, HttpResponse& res) {
            std::string data = R"({"data":"call coin server"})";
            res.json(200, data);
        });

        server->get("/a/b/:id/get", [](HttpRequest& req, HttpResponse& res) {
            auto id = req.get_params("id");
            std::string data;
            data.append(R"({"id":)").append(R"(")").append(id).append(R"("})");
            res.json(200, data);
        });

        server->listen_and_serve_tls(&config);
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread_pool.addTask([](){

        arch_net::coin::CoinClient client("https://127.0.0.1:8888",
                                          "../../arch_net/test/mtls_test/keys/ca/ca_cert.pem",
                                          "../../arch_net/test/mtls_test/keys/client/client_cert.pem",
                                          "../../arch_net/test/mtls_test/keys/client/private/client_key.pem");

        auto session = client.new_session();
        if (!session) {
            std::cout << "client connection error";
            return ;
        }
        auto result = session->Get("/a/b/c", {}, {}, nullptr);
        if (result.error() == Error::Success) {
            std::cout << result.value().body << std::endl;
        } else {
            std::cout << " error" << std::endl;
        }

    });

    acl::fiber_tbox<bool> wait;
    wait.pop();
}

TEST(HTTPS, Test_WSS_server_client)
{
    IOWorkerPool thread_pool;

    thread_pool.addTask([](){

        google::InitGoogleLogging("test");
        FLAGS_logtostderr = true;

        arch_net::ServerConfig config;
        config.ip_addr = "127.0.0.1";
        config.port = 8888;
        config.uds_path = "";
        config.tls_ca_path = "../../arch_net/test/mtls_test/keys/ca/ca_cert.pem";
        config.tls_cert_path = "../../arch_net/test/mtls_test/keys/server/server_cert.pem";
        config.tls_key_path = "../../arch_net/test/mtls_test/keys/server/private/server_key.pem";
        config.io_thread_num = FLAGS_io_thread_num;

        auto server = new_coin_server();
        defer(delete server);

        server->get("/a/b/c", [](HttpRequest& req, HttpResponse& res) {
            std::string data = R"({"data":"call coin server"})";
            res.json(200, data);
        });

        server->get("/a/b/:id/get", [](HttpRequest& req, HttpResponse& res) {
            auto id = req.get_params("id");
            std::string data;
            data.append(R"({"id":)").append(R"(")").append(id).append(R"("})");
            res.json(200, data);
        });


        WebSocketCallback endpoint;
        endpoint.OnMessage = [&](WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {

            std::cout << buff->ToString() << std::endl;
            uint64_t i = 0;
            while (i < UINT32_MAX/1000){
                i++;
            }
            conn->sendText(buff->ToString());
            conn->sendText(buff->ToString());
            conn->sendText(buff->ToString());
            conn->sendText(buff->ToString());
            conn->sendText(buff->ToString());

            return true;
        };
        server->websocket("/ws", endpoint);

        server->listen_and_serve_tls(&config);
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    thread_pool.addTask([](){

        arch_net::coin::CoinClient client("https://127.0.0.1:8888",
                                          "../../arch_net/test/mtls_test/keys/ca/ca_cert.pem",
                                          "../../arch_net/test/mtls_test/keys/client/client_cert.pem",
                                          "../../arch_net/test/mtls_test/keys/client/private/client_key.pem");

        auto session = client.new_session();
        WebSocketCallback callback;
        callback.OnOpen = [&](WebSocketConnection* conn){
            conn->sendText("hello world");
            return true;
        };
        callback.OnMessage = [&](WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {
            std::cout << buff->ToString() << std::endl;

            //conn->closeWebsocket();
            return true;
        };

        auto result = session->WebSocket("/ws", {}, {}, callback);
        if (result.error() == Error::Success) {
            std::cout << result.value().body << std::endl;
        } else {
            std::cout << " error" << std::endl;
        }
    });

    acl::fiber_tbox<bool> wait;
    wait.pop();
}


static int test_websocket_handler(arch_net::ISocketStream* stream) {
    arch_net::coin::WebSocketCallback callback;
    callback.OnMessage = [&](arch_net::coin::WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {

        LOG(INFO) << "server recv: " << buff->ToString() ;
        conn->sendText(buff->ToString());
        conn->sendText(buff->ToString());
        conn->sendText(buff->ToString());
        conn->sendText(buff->ToString());
        conn->sendText(buff->ToString());
        return true;
    };

    IOWorkerPool pool(1);
    auto connection = std::make_shared<arch_net::coin::WebSocketConnection>
            (stream, callback, &pool);
    connection->process();

    return 0;
}

TEST(Test_Mux, Test_Mux_With_Websocket)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    IOWorkerPool server_pool(1);
    server_pool.addTask([&]() {
        auto tcp_server = new_tcp_socket_server();
        auto mux_server = arch_net::mux::MultiplexingSocketServer(tcp_server, true);

        mux_server.init("127.0.0.1", 18888);
        mux_server.set_handler(test_websocket_handler);
        mux_server.start(1);
    });

    IOWorkerPool client_pool(1);

    client_pool.addTask([&](){

        auto client = new_tcp_socket_client();
        auto mux_client = std::make_unique<arch_net::mux::MultiplexingSocketClient>(client, true);

        auto stream = mux_client->connect("127.0.0.1", 18888);
        arch_net::coin::WebSocketCallback callback;
        callback.OnOpen = [&](arch_net::coin::WebSocketConnection* conn){
            conn->sendText("1234432112334");
            return true;
        };
        callback.OnMessage = [&](arch_net::coin::WebSocketConnection* conn, arch_net::Buffer* buff, bool)->bool {

            LOG(INFO) << "client recv: "<< buff->ToString();
            return true;
        };

        IOWorkerPool pool(1);
        auto connection = std::make_shared<arch_net::coin::WebSocketConnection>
                (stream, callback, &pool);
        connection->process();

        delete stream;

        LOG(INFO) << "client exit";
    });
}