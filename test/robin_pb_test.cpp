
#include "echo.pb.h"
#include "../rpc/robin_pbrpc.h"
#include "../rpc/pbrpc_controller.h"
#include "../rpc/pbrpc_channel.h"
#include "gtest/gtest.h"
#include "glog/logging.h"


class MyEchoService : public example::EchoService {
public:
    virtual void Echo(::google::protobuf::RpcController* c,
                      const ::example::EchoRequest* request,
                      ::example::EchoResponse* response,
                      ::google::protobuf::Closure* done) {

        auto controller = dynamic_cast<arch_net::robin::RobinPBrpcController*>(c);
        defer(delete controller);
        defer(delete request);
        defer(delete response);

        std::cout << request->message() << std::endl;
        response->set_message(
                std::string("I have received '") + request->message() + std::string("'"));

        if (controller->RemoteUseStreaming()) {
            controller->SetStreaming();
        }

        done->Run();

        // get streaming

        auto streaming = controller->GetStreamingConnection();

        ::example::EchoRequest streaming_request;

        auto ret = streaming->recv(&streaming_request);
        LOG(INFO) << "server recv streaming message: " << streaming_request.DebugString();

        for (int i = 0; i < 100; i++) {
            auto ret = streaming->send(response);
            if (!ret) {
                break;
            }
            acl_fiber_sleep(1);
            LOG(INFO) << "server send streaming, ret: "<< ret;
        }


        streaming->close_streaming();
        LOG(INFO) << "closed streaming" << std::endl;
    }
};//MyEchoService

TEST(Test_RPC, Test_Robin_PBrpc)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;


    std::thread server_thread([]() {
        arch_net::ServerConfig config;
        config.ip_addr = "127.0.0.1";
        config.port = 8888;
        config.io_thread_num = 4;

        arch_net::robin::RobinPBrpcServer server;
        MyEchoService echo_service;
        server.add_service(&echo_service);

        server.listen_and_serve(&config);
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    {
        example::EchoRequest request;
        example::EchoResponse response;
        request.set_message("hello, myrpc.");

        arch_net::ClientOption option;
        arch_net::robin::RobinPBrpcChannel channel;
        channel.init("127.0.0.1", 8888, option);

        example::EchoService_Stub stub(&channel);

        arch_net::robin::RobinPBrpcController controller;

        controller.SetStreaming();
        stub.Echo(&controller, &request, &response, nullptr);
        if (controller.Failed()) {
            std::cout << "client error" << ": " << controller.ErrorText() << std::endl;
            return;
        }
        std::cout << "resp:" << response.message() << std::endl;

        auto streaming = controller.GetStreamingConnection();

        auto ret = streaming->send(&request);
        while (true) {
            ret = streaming->recv(&response);
            LOG(INFO) << "client recv streaming message: " << response.DebugString();
            if (!ret)
                break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));

}

TEST(Test_RPC, Test_Robin_PBrpc_With_Mux)
{

    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    std::thread server_thread([]() {
        arch_net::ServerConfig config;
        config.ip_addr = "127.0.0.1";
        config.port = 8888;
        config.io_thread_num = 4;
        config.connection_type = arch_net::ServerConnectionType::Multiplexing;

        arch_net::robin::RobinPBrpcServer server;
        MyEchoService echo_service;
        server.add_service(&echo_service);

        server.listen_and_serve(&config);
    });
    server_thread.join();
}

TEST(ele,e) {

}

TEST(Test_RPC, Test_client)
{
    std::this_thread::sleep_for(std::chrono::seconds(2));

    {
        example::EchoRequest request;
        example::EchoResponse response;
        request.set_message("hello, myrpc.");

        arch_net::ClientOption option;
        option.connection_type = arch_net::ConnectionType::Multiplexing;

        arch_net::robin::RobinPBrpcChannel channel;
        channel.init("127.0.0.1", 8888, option);

        example::EchoService_Stub stub(&channel);

        arch_net::robin::RobinPBrpcController controller;

        controller.SetStreaming();
        stub.Echo(&controller, &request, &response, nullptr);
        if (controller.Failed()) {
            std::cout << "client error" << ": " << controller.ErrorText() << std::endl;
            return;
        }
        std::cout << "resp:" << response.message() << std::endl;

        auto streaming = controller.GetStreamingConnection();

        auto ret = streaming->send(&request);

        while (true) {
            ret = streaming->recv(&response);
            LOG(INFO) << "client recv streaming message: " << response.DebugString();
            if (!ret)
                break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}


