#pragma once
#include "shared_state.h"
#include "../defer.h"
#include "fiber/fiber_tbox.hpp"
namespace arch_net{

template <typename Result, typename Type>
class Future;

template <typename Result, typename Type>
class Promise {
public:
    Promise() : state_(std::make_shared<SharedState<Result, Type> >()) {}

    bool setValue(Result result, const Type& value) const {
        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();

        if (state->status != FutureStatus::None) {
            return false;
        }

        state->value = value;
        state->result = result;
        state->status = FutureStatus::Done;

        decltype(state->then_callback) callbacks;
        callbacks.swap(state->then_callback);

        state->mutex.unlock();

        for (auto& callback : callbacks) {
            if (callback) callback();
        }

        state->condition.notify();
        return true;
    }

    bool setTimeout() const {
        static Type DEFAULT_VALUE;
        static Result DEFAULT_RESULT;

        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        defer(state->mutex.unlock());
        state->value = DEFAULT_VALUE;
        state->result = DEFAULT_RESULT;
        state->status = FutureStatus::Timeout;
        return true;
    }

    bool isComplete() const {
        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        defer(state->mutex.unlock());
        return state->status == FutureStatus::Done;
    }

    bool isTimeout() const {
        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        defer(state->mutex.unlock());
        return state->status == FutureStatus::Timeout;
    }

    Future<Result, Type> getFuture() const { return Future<Result, Type>(state_); }

private:
    std::shared_ptr<SharedState<Result, Type> > state_;
};

}
