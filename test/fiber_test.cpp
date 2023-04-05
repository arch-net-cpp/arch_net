
#include <fiber/libfiber.hpp>
#include <gtest/gtest.h>
#include "../common.h"
#include "../socket.h"
#include <fiber/fiber_tbox.hpp>
#include <fiber/channel.hpp>
#include <fiber/fiber_channel.h>
#include "fiber/fiber.hpp"
#include "../utils/fiber.h"


acl::fiber_tbox<bool> fiber_tbox;

TEST(FIBER_TEST, Test_fiber)
{
    ACL_FIBER* f1;

    acl::fiber_tbox<bool> box;

    std::thread thread(
            [&](){

                auto f1 = create_fiber([&] {
                    int fd = arch_net::socket();
                    arch_net::listen(fd, "127.0.0.1", 18888);
                    std::cout << "start accept" << std::endl;
                    int cfd = arch_net::accept(fd);
                    std::cout << "end accept" << std::endl;
                    char buf[100];
                    int ret = arch_net::read(cfd, buf, 100);

                    std::cout << "end pop" << std::endl;
                    return 0;
                });

                auto f2 = create_fiber([&] {
//                    int fd = arch_net::socket();
//                    arch_net::listen(fd, "127.0.0.1", 18888);
//                    std::cout << "start accept" << std::endl;
//                    arch_net::accept(fd);
                    std::cout << "start f2" << std::endl;
                    acl_fiber_sleep(4);
                    acl_fiber_kill(f1);
                    std::cout << acl_fiber_killed(f1) << std::endl;
                    std::cout << "kill f1" << std::endl;
                    return 0;
                });

                std::cout << "start schedule" << std::endl;
                acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
                std::cout << "stop schedule" << std::endl;
            });

    std::this_thread::sleep_for(std::chrono::seconds(5));
    thread.join();
}


TEST(FIBER_TEST, Test_fiber_channel)
{
    acl::fiber_tbox<int> box;

    auto chn = acl_channel_create(sizeof(int), 1);
    defer(acl_channel_free(chn));

    auto f1 = create_fiber([&] {

        while (true) {
            std::cout << box.pop() << std::endl;
            int val;
            int ret ;//= acl_channel_recv(chn, &val);
            std::cout << "get ret: " << 1 << ", value: " << val << std::endl;
        }
        return 0;
    });


    auto f = create_fiber([&]() {
        acl_fiber_sleep(2);
        acl_fiber_sleep(1);
        int val = 100;
        box.push(new int(100));
        acl_fiber_kill(f1);
        std::cout << "kill f1" << std::endl;
        return 0;
    });

    std::cout << "start schedule" << std::endl;
    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
    std::cout << "stop schedule" << std::endl;

}


TEST(FIBER_TEST, Test_fiber_poll)
{
    int p1[2]; // p[0] in  p[1] out
    if (pipe(p1) < 0) {
        return;
    }

    int p2[2]; // p[0] in  p[1] out
    if (pipe(p2) < 0) {
        return;
    }

    go[&]() {
        struct pollfd fds[2];
        memset (fds, 0, sizeof(fds));
        fds[0].fd = p1[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        fds[1].fd = p2[0];
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        while (true) {
            int pollret = acl_fiber_poll(fds, 2, -1);
            if (pollret > 0) {
                if (fds[0].revents & POLLIN) {
                    char buf;
                    read(p1[0], &buf, 1);
                    std::cout << "recv p1 signal" << std::endl;
                }
                if (fds[1].revents & POLLIN) {
                    // recv exit signal
                    std::cout << "recv p2 signal" << std::endl;
                    break;
                }
            } else if (pollret == 0) {
                /* the poll has timed out, nothing can be read or written */
                break;
            } else {
                /* the poll failed */
                std::cout << ("poll failed") << std::endl;
                break;
            }
        }
    };

    go[&]() {
        acl_fiber_sleep(2);
        write(p1[1], " ", 1);
    };

    go[&]() {
        acl_fiber_sleep(5);
        write(p2[1], " ", 1);
    };


    std::cout << "start schedule" << std::endl;
    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
    std::cout << "stop schedule" << std::endl;
}

int recv_loop() {
    int p[2]; // p[0] in  p[1] out
    if (pipe(p) < 0) {
        return false;
    }
    struct pollfd fds[2];
    fds[0].fd = 1;//stream->get_fd();
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    fds[1].fd = p[0];
    fds[1].events = POLLIN;
    fds[1].revents = 0;
    while (true) {
        int pollret = acl_fiber_poll(fds, 2, -1);
        if (pollret > 0) {
            if (fds[0].revents & POLLIN) {
                if (!1) {
                    break;
                }
            }
            if (fds[1].revents & POLLIN) {
                // recv exit signal
                break;
            }
        } else if (pollret == 0) {
            /* the poll has timed out, nothing can be read or written */
        } else {
            /* the poll failed */
            perror("poll failed");
        }
    }
}