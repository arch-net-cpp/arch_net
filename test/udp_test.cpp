
#include <gtest/gtest.h>
#include "../socket.h"
#include "fiber/go_fiber.hpp"
#include "../udp/udp_socket_stream.h"

void setFdpro(int fd, int property){
    int sockopt = 1;
    if (setsockopt(fd, SOL_SOCKET, property, (void* )&sockopt, sizeof(sockopt)) < 0) {
        exit(1);
    }
}

TEST(Test_upd, test_1)
{

    std::thread server([](){
            int port = 18888;
            int listenfd = socket(AF_INET, SOCK_DGRAM, 0);
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof addr);
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;
            int r = ::bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr));
            printf(" bind port = %d \n", port);
            int sockopt = 1;
            if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void* )&sockopt, sizeof(sockopt)) < 0) {
                return ;
            }
            if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, (void* )&sockopt, sizeof(sockopt)) < 0) {
                return;
            }

            while (true) {
                struct sockaddr_in client_addr;
                socklen_t rsz = sizeof(client_addr);
                size_t size = 1024;
                char recvbuf[size];
                auto recv_sive = recvfrom(listenfd, recvbuf,sizeof(recvbuf),0,(struct sockaddr * )&client_addr, &rsz);
                if (recv_sive < 0) {
                    return;
                }
                printf(" content is : %s\n", recvbuf);
                printf("man thread recvfrom %s:%d\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));


                int r = 0;
                while(recv_sive > 0){
                    r = ::sendto(listenfd, recvbuf+r, recv_sive, 0, (struct sockaddr * )&client_addr,rsz);
                    recv_sive -= r;
                }

                /*
                std::thread* sub_client = new std::thread([client_addr = client_addr, port = port]() {

                    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
                    setFdpro(client_fd,SO_REUSEADDR);
                    setFdpro(client_fd,SO_REUSEPORT);
                    struct sockaddr_in local_addr;
                    memset(&local_addr, 0, sizeof local_addr);
                    local_addr.sin_family = AF_INET;
                    local_addr.sin_port = htons(port);
                    local_addr.sin_addr.s_addr = INADDR_ANY;

                    int ret = ::bind(client_fd, (struct sockaddr *) &local_addr, sizeof(struct sockaddr));
                    if (ret < 0) {
                        return ;
                    }
                    ret = connect(client_fd , (struct sockaddr * )&client_addr, sizeof(struct sockaddr));
                    if (ret < 0) {
                        return ;
                    }
                    printf(" create fd: %d to %s:%d\n",client_fd, inet_ntoa(client_addr.sin_addr),port);
                    //连接成功之后，发送消息
                    char send_buf[1024]="I am server";
                    int r = ::sendto(client_fd,send_buf,sizeof(send_buf),0,(struct sockaddr * )&client_addr,sizeof(client_addr));

                    printf(" sendto %s -> %s:%d \n",send_buf,inet_ntoa(client_addr.sin_addr),port);

                    while (true) {
                        struct sockaddr_in raddr;
                        socklen_t rsz = sizeof(raddr);
                        size_t size = 1024;
                        char recvbuf[size];
                        int recv_sive = ::recvfrom(client_fd,recvbuf,sizeof(recvbuf),0,(struct sockaddr * )&raddr, &rsz);

                        printf("read %d bytes\n", recv_sive);
                        printf(" content is : %s\n", recvbuf);

                        int r = 0;
                        while(recv_sive > 0){
                            r = ::sendto(client_fd, recvbuf+r, recv_sive, 0, (struct sockaddr * )&raddr,rsz);
                            recv_sive -= r;
                        }
                    }
                });
                */
                 }

        });

    std::this_thread::sleep_for(std::chrono::seconds(1));


    printf("1\n");
    std::string serverIp = "127.0.0.1";

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof local_addr);
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(0);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    int r = ::bind(fd, (struct sockaddr *) &local_addr, sizeof(struct sockaddr));
    if (r < 0) {
        printf("bind error\n");
        return;
    }


    struct sockaddr_in other_addr;
    memset(&other_addr, 0, sizeof other_addr);
    other_addr.sin_family = AF_INET;
    other_addr.sin_port = htons(18888);
    other_addr.sin_addr.s_addr = inet_addr(serverIp.c_str());

    int i = 2;
    char buf[20] = "I am client";
    printf("client start\n");
    while(1){
        int r = sendto(fd, buf, strlen(buf)+1, 0,(struct sockaddr * )&other_addr,sizeof(other_addr));
        printf("client send result %d\n", r);

        struct sockaddr_in other;
        socklen_t rsz = sizeof(other);
        char recv_buf[1024];
        int recv_sive = recvfrom(fd,recv_buf,sizeof(recv_buf),0,(struct sockaddr * )&other,&rsz);
        printf("client read %d bytes  context is : %s\n", recv_sive, recv_buf);
        sleep(1);
    }
    close(fd);


}


static int udp_test_handler(arch_net::ISocketStream* stream) {
    LOG(INFO) << "new connection";
    arch_net::Buffer buffer(2048, 0);
    while (true) {
        auto size = buffer.ReadFromSocketStream(stream);
        if (size < 0) {
            return -1;
        }

        std::cout <<"server recv :" << buffer.ToString() << std::endl;

        size = stream->send(buffer.data(), buffer.size());
        if (size < 0) {
            return -1;
        }
        buffer.Retrieve(size);
    }
    return 0;
}

TEST(Test_UDP, test_2)
{
    google::InitGoogleLogging("test");
    FLAGS_logtostderr = true;

    std::thread thread([]() {

        auto server = arch_net::UDPSocketServer();
        server.init("127.0.0.1", 18888);
        server.set_handler([](arch_net::ISocketStream* stream)->int {
            return udp_test_handler(stream);
        });

        server.start(0);
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
        go[](){
            auto client = arch_net::UDPSocketClient();
            auto stream = client.connect("127.0.0.1", 18888);
            //acl_fiber_delay(100);
            int index = 1;
            while (true) {

                std::string msg = "I am client " + std::to_string(index++);

                auto n = stream->send(msg.c_str(), msg.size());
                if (n < 0) {
                    break;
                }
                std::cout << "send done"<< std::endl;

                char recv[1048] = {0};
                n = stream->recv(recv, 1048);
                if (n < 0) {
                    break;
                }
                std::cout << recv << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            delete stream;
        };
        acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
    }

}