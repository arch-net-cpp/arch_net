#include <gtest/gtest.h>

#include "../utils/future/future.h"
#include "fiber/go_fiber.hpp"
using namespace arch_net;

enum class ErrorCode { kSuccess = 0, kError, kUserCallbackException };


TEST(TEST_FUTURE, test_future_in_fiber1)
{
    Promise<ErrorCode, int> promise;
    auto future = promise.getFuture();
    go[future = std::move(future)]() mutable {
        ErrorCode r;
        int value;
        r = future.get(value);
        std::cout << "future done";
    };

    promise.setValue(ErrorCode::kSuccess, 10);
    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}


TEST(TEST_FUTURE, test_future_in_fiber2)
{
    Promise<ErrorCode, int> promise;
    go[&]() {
        std::cout << "in fiber" << std::endl;
        acl_fiber_sleep(2);
        promise.setValue(ErrorCode::kSuccess, 10);
        std::cout << "set value" << std::endl;
    };

    auto future = promise.getFuture();
    go[future = std::move(future)]() mutable {
        ErrorCode r;
        int value;
        r = future.get(value);
        std::cout << "future done";
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}


TEST(TEST_FUTURE, test_future_timeout)
{
    Promise<ErrorCode, int> promise;
    go[&]() {
        std::cout << "in fiber" << std::endl;
        acl_fiber_sleep(2);
        promise.setValue(ErrorCode::kSuccess, 10);
        std::cout << "set value" << std::endl;
        acl_fiber_sleep(1);
    };

    auto future = promise.getFuture();
    go[future = std::move(future)]() mutable {
        ErrorCode r;
        int value;
        auto ret = future.get(r, value, 1000);
        EXPECT_EQ(false, ret);
        std::cout << "future done" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}

Future<ErrorCode, int> call(Promise<ErrorCode, int>& promise) {
    auto f = promise.getFuture();
    return f.then([](ErrorCode e, const int& value) -> std::pair<ErrorCode, int> {
        std::cout<< "in future then" << std::endl;

        return {ErrorCode::kSuccess, 10};
    });
}

TEST(TEST_FUTURE, test_future_then1)
{
    Promise<ErrorCode, int> promise;
    go[&]() {
        std::cout << "in fiber" << std::endl;
        acl_fiber_sleep(2);
        promise.setValue(ErrorCode::kSuccess, 10);
        std::cout << "set value" << std::endl;
        acl_fiber_sleep(1);
    };

    auto future = call(promise);
    go[future = std::move(future)]() mutable {
        ErrorCode r;
        int value;
        auto ret = future.get(r, value, 1000);
        EXPECT_EQ(false, ret);
        std::cout << "future timeout" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}

TEST(TEST_FUTURE, test_future_then2)
{
    Promise<ErrorCode, int> promise;
    go[&]() {
        std::cout << "in fiber" << std::endl;
        promise.setValue(ErrorCode::kSuccess, 10);
        std::cout << "set value" << std::endl;
        acl_fiber_sleep(1);
    };

    auto future = call(promise);
    go[future = std::move(future)]() mutable {
        ErrorCode r;
        int value;
        auto ret = future.get(r, value, 1000);
        EXPECT_EQ(true, ret);
        std::cout << "future done" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}


TEST(TEST_FUTURE, test_future_waitAny)
{
    Promise<ErrorCode, int> p1;
    Promise<ErrorCode, int> p2;
    Promise<ErrorCode, int> p3;
    Promise<ErrorCode, int> p4;

    go[&]() {
        acl_fiber_delay(100);
        p1.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p1 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(200);
        p2.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p2 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(300);
        p3.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p3 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(400);
        p4.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p4 set value" << std::endl;
        acl_fiber_sleep(1);
    };

    std::vector<Future<ErrorCode, int>> futures;

    futures.emplace_back(p1.getFuture());
    futures.emplace_back(p2.getFuture());
    futures.emplace_back(p3.getFuture());
    futures.emplace_back(p4.getFuture());

    go[&]() mutable {
        auto future = waitAny<ErrorCode, int>(futures.begin(), futures.end());
        int val;
        auto r = future.get(val);
        std::cout << "future done" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}


TEST(TEST_FUTURE, test_future_waitAnyN)
{
    Promise<ErrorCode, int> p1;
    Promise<ErrorCode, int> p2;
    Promise<ErrorCode, int> p3;
    Promise<ErrorCode, int> p4;

    go[&]() {
        acl_fiber_delay(100);
        p1.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p1 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(200);
        p2.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p2 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(300);
        p3.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p3 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(400);
        p4.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p4 set value" << std::endl;
        acl_fiber_sleep(1);
    };

    std::vector<Future<ErrorCode, int>> futures;

    futures.emplace_back(p1.getFuture());
    futures.emplace_back(p2.getFuture());
    futures.emplace_back(p3.getFuture());
    futures.emplace_back(p4.getFuture());

    go[&]() mutable {
        auto future = waitAnyN<ErrorCode, int>(futures.begin(), futures.end(), 2);
        std::vector<int> val;
        auto r = future.get(val);
        std::cout << "future done" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}


TEST(TEST_FUTURE, test_future_waitAll)
{
    Promise<ErrorCode, int> p1;
    Promise<ErrorCode, int> p2;
    Promise<ErrorCode, int> p3;
    Promise<ErrorCode, int> p4;

    go[&]() {
        acl_fiber_delay(100);
        p1.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p1 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(200);
        p2.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p2 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(300);
        p3.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p3 set value" << std::endl;
        acl_fiber_sleep(1);
    };
    go[&]() {
        acl_fiber_delay(400);
        p4.setValue(ErrorCode::kSuccess, 10);
        std::cout << "p4 set value" << std::endl;
        acl_fiber_sleep(1);
    };

    std::vector<Future<ErrorCode, int>> futures;

    futures.emplace_back(p1.getFuture());
    futures.emplace_back(p2.getFuture());
    futures.emplace_back(p3.getFuture());
    futures.emplace_back(p4.getFuture());

    go[&]() mutable {
        auto future = waitAll<ErrorCode, int>(futures.begin(), futures.end());
        std::vector<int> val;
        auto r = future.get(val);
        std::cout << "future done" << std::endl;
    };

    acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
}
