#pragma once
#include <utility>

#include "iostream"
#include "fiber/fiber_base.h"
#include "non_copyable.h"
#include "defer.h"
#include "fiber/wait_group.hpp"
#include "fiber/fiber.hpp"
#include "fiber/go_fiber.hpp"

class fiber_ctx {
public:
    fiber_ctx(const std::function<int()>& fn) {
        fn_ = std::move(fn);
    }
    ~fiber_ctx() = default;
    std::function<int()> fn_;
};


class FiberCtx {
public:
    FiberCtx() {};
    FiberCtx(std::function<void()> fn) {
        fn_ = std::move(fn);
    }
    ~FiberCtx() = default;

    std::function<void()> fn_;
};

class FiberIntCtx {
public:
    FiberIntCtx() {};
    FiberIntCtx(std::function<int()> fn) {
        fn_ = std::move(fn);
    }
    ~FiberIntCtx() = default;

    std::function<int()> fn_;
};

class GoFiber : public arch_net::TNonCopyable{
public:
    GoFiber(void) {}
    GoFiber(size_t stack_size) : stack_size_(stack_size) {}

    ACL_FIBER* operator > (std::function<void()>&& fn)
    {
        auto* ctx = new FiberCtx(std::move(fn));
        return acl_fiber_create(fiber_main, (void*) ctx, stack_size_);
    }

private:
    size_t stack_size_ = 320000;

    static void fiber_main(ACL_FIBER*, void* ctx)
    {
        auto* fc = (FiberCtx *) ctx;
        defer(delete fc);
        fc->fn_();
    }
};

static void kill_fiber(ACL_FIBER* f) {
    if (acl_fiber_killed(f)) return;

    acl_fiber_kill(f);
}

static void fiber_main(ACL_FIBER*, void* ctx) {
    auto* fc = (fiber_ctx *) ctx;
    fc->fn_();
    delete fc;
}

static ACL_FIBER* create_fiber(const std::function<int()>& fn) {
    auto* ctx = new fiber_ctx(fn);
    return acl_fiber_create(fiber_main, (void*) ctx, 320000);
}

static ACL_FIBER* create_timer(int milliseconds, const std::function<int()>& call_back) {
    auto* ctx = new fiber_ctx(call_back);
    return acl_fiber_create_timer(milliseconds, 320000, fiber_main, ctx);
}




//void cancel_timer(ACL_FIBER* fiber) {
//    acl_fiber_set_errno(fiber, 1001);
//    acl_fiber_reset_timer(fiber, 0);
//}

#define	coro GoFiber()>
