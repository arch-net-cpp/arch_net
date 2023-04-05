#pragma once

#include "iostream"
#include "fiber/fiber_mutex.hpp"
#include "fiber/fiber_cond.hpp"
#include "list"

namespace arch_net{

enum class FutureStatus { None, Timeout, Done };

template <typename Result, typename Type>
struct SharedState {
    acl::fiber_mutex mutex;
    acl::fiber_cond  condition;

    Result result;
    Type value;

    FutureStatus status;

    std::list<typename std::function<void()> > then_callback;
};
}