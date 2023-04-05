
#include <gtest/gtest.h>
#include "../socket.h"
#include "../common.h"

TEST(TEST_NET, Test_net)
{
    go[]() {
        auto sock = arch_net::socket();
        arch_net::listen(sock, "127.0.0.1", 18888);

        while (true) {
            auto cfd = arch_net::accept(sock);
            go[&]() {
                std::cout <<"new connection"<< std::endl;
                while (true) {

                    char buf[1024] = {0};
                    int n = arch_net::read(cfd, buf, 1024);
                    std::cout <<"recv "<< n << " " << buf << std::endl;

                    if (n <= 0) {
                        break;
                    }
                    n = arch_net::write(cfd, buf, n);
                    if (n <= 0) {
                        break;
                    }
                    std::cout <<"send "<< n << " " << std::endl;
                }
                std::cout <<"connection closed"<< std::endl;
            };
        }

    };

    acl::fiber::schedule();

}