
#ifndef LIB_FUTURE_H_
#define LIB_FUTURE_H_

#include <functional>
#include <list>
#include <memory>
#include "../worker_pool.hpp"
#include "shared_state.h"
#include "promise.h"

namespace arch_net {

template <typename Result, typename Type>
class Future {
public:
    typedef std::function<std::pair<Result, Type>(Result, const Type&)> ThenCallback;
    typedef std::function<void(Result, const Type&)> ThenVoidCallback;

    typedef std::shared_ptr<SharedState<Result, Type> > InternalStatePtr;

    Future<Result, Type> then(ThenCallback callback, WorkerPool* pool = nullptr) {
        Promise<Result, Type> new_promise;
        auto new_future = new_promise.getFuture();

        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        auto new_state = state_;
        if (state->status == FutureStatus::Done) {
            state->mutex.unlock();
            ExecuteTask(std::move(new_promise), new_state, callback, pool);
        } else if (state->status == FutureStatus::Timeout){
            state->mutex.unlock();
            new_promise.setTimeout();
        } else {
            state_->then_callback.emplace_back(
                [new_promise = std::move(new_promise), callback = std::move(callback), new_state, pool, this] () mutable {
                    ExecuteTask(new_promise, new_state, callback, pool);
                });
            state->mutex.unlock();
        }

        return new_future;
    }

    Future<Result, Type> thenVoid(ThenVoidCallback callback, WorkerPool* pool = nullptr) {
        Promise<Result, Type> new_promise;
        auto new_future = new_promise.getFuture();

        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        auto new_state = state_;
        if (state->status == FutureStatus::Done) {
            state->mutex.unlock();
            ExecuteTask(std::move(new_promise), new_state, callback, pool);
        } else if (state->status == FutureStatus::Timeout){
            state->mutex.unlock();
            new_promise.setTimeout();
        } else {
            state_->then_callback.emplace_back(
                    [new_promise = std::move(new_promise), callback = std::move(callback), new_state, pool, this] () mutable {
                        ExecuteTask(new_promise, new_state, callback, pool);
                    });
            state->mutex.unlock();
        }

        return new_future;
    }

    Result get(Type& value) const {
        SharedState<Result, Type>* state = state_.get();
        state->mutex.lock();
        defer(state->mutex.unlock());

        if (state->status == FutureStatus::None) {
            // Wait for result
            while (state->status == FutureStatus::None) {
                state->condition.wait(state->mutex);
            }
        }

        value = state->value;
        return state->result;
    }

    template <typename Duration>
    bool get(Result& res, Type& value, Duration d) const {
        SharedState<Result, Type>* state = state_.get();

        state->mutex.lock();
        defer(state->mutex.unlock());

        if (state->status == FutureStatus::None) {
            // Wait for result
            while (state->status == FutureStatus::None) {
                if (!state->condition.wait(state->mutex, d)) {
                    // Timeout while waiting for the future to complete
                    state->status = FutureStatus::Timeout;
                    return false;
                }
            }
        }
        if (state->status == FutureStatus::Timeout) {
            return false;
        }

        value = state->value;
        res = state->result;
        return true;
    }


private:
    void ExecuteTask(Promise<Result, Type> promise, InternalStatePtr state,
                     ThenCallback callback, WorkerPool* pool) {

        auto task = [promise = std::move(promise), state = std::move(state),
                callback = std::move(callback)]() {
            if (state->status == FutureStatus::Timeout) {
                promise.setTimeout();
                return ;
            }
            if (promise.isTimeout()) {
                return ;
            }
            auto ret = callback(state->result, state->value);
            promise.setValue(ret.first, ret.second);
        };
        if (pool) {
            pool->addTask(std::move(task));
            return;
        }
        // use future default workerpool
        GlobalIOWorkerPool::GetPool()->addTask(std::move(task));
        return;
    }

    void ExecuteTask(Promise<Result, Type> promise, InternalStatePtr state,
                     ThenVoidCallback callback, WorkerPool* pool) {

        auto task = [promise = std::move(promise), state = std::move(state),
                callback = std::move(callback)]() {
            if (state->status == FutureStatus::Timeout) {
                promise.setTimeout();
                return ;
            }
            if (promise.isTimeout()) {
                return ;
            }
            callback(state->result, state->value);
            promise.setValue(state->result, state->value);
        };
        if (pool) {
            pool->addTask(std::move(task));
            return;
        }
        // use future default workerpool
        GlobalIOWorkerPool::GetPool()->addTask(std::move(task));
        return;
    }

private:
    Future(InternalStatePtr state) : state_(state) {}

    std::shared_ptr<SharedState<Result, Type> > state_;

    template <typename U, typename V>
    friend class Promise;
};

template<typename Result, typename Type, typename Iterator>
static Future<std::vector<Result>, std::vector<Type>> waitAll(Iterator begin, Iterator end, WorkerPool* pool = nullptr) {
    if (begin == end) {
        Promise<std::vector<Result>, std::vector<Type>> promise;
        promise.setValue({}, {});
        return promise.getFuture();
    }
    struct AllContext {
        AllContext(int n) : results(n), values(n), size(n){}
        Promise<std::vector<Result>, std::vector<Type>> promise;
        std::vector<Result> results;
        std::vector<Type> values;
        size_t count{0};
        size_t size;
        acl::fiber_mutex mutex;
    };
    auto ctx = std::make_shared<AllContext>(std::distance(begin, end));

    for (size_t i = 0; begin != end; ++begin, ++i) {
        begin->thenVoid([ctx, i](Result r, const Type& v) {
            ctx->mutex.lock();
            defer(ctx->mutex.unlock());
            ctx->results[i] = r;
            ctx->values[i]  = v;
            ctx->count++;
            if (ctx->size == ctx->count) {
                ctx->promise.setValue(ctx->results, ctx->values);
            }
        }, pool);
    }

    return ctx->promise.getFuture();
}


template<typename Result, typename Type, typename Iterator>
static Future<Result, Type> waitAny(Iterator begin, Iterator end, WorkerPool* pool = nullptr) {
    if (begin == end) {
        Promise<Result, Type> promise;
        promise.setValue({}, {});
        return promise.getFuture();
    }
    struct AnyContext {
        AnyContext() {}
        Promise<Result, Type> promise;
        bool done{false};
        acl::fiber_mutex mutex;
    };
    auto ctx = std::make_shared<AnyContext>();

    for (size_t i = 0; begin != end; ++begin, ++i) {
        begin->thenVoid([ctx](Result r, const Type& v) {
            ctx->mutex.lock();
            defer(ctx->mutex.unlock());
            if (!ctx->done) {
                ctx->promise.setValue(r, v);
                ctx->done = true;
            }
        }, pool);
    }

    return ctx->promise.getFuture();
}


template<typename Result, typename Type, typename Iterator>
static Future<std::vector<Result>, std::vector<Type>> waitAnyN(Iterator begin, Iterator end, size_t N, WorkerPool* pool = nullptr) {
    if (begin == end || N == 0) {
        Promise<std::vector<Result>, std::vector<Type>> promise;
        promise.setValue({}, {});
        return promise.getFuture();
    }
    struct AnyNContext {
        AnyNContext(int n) : results(n), values(n), size(n){}
        Promise<std::vector<Result>, std::vector<Type>> promise;
        std::vector<Result> results;
        std::vector<Type> values;
        size_t count{0};
        size_t size;
        acl::fiber_mutex mutex;
    };
    auto ctx = std::make_shared<AnyNContext>(N);

    for (size_t i = 0; begin != end; ++begin, ++i) {
        begin->thenVoid([ctx, i](Result r, const Type& v) {
            ctx->mutex.lock();
            defer(ctx->mutex.unlock());
            ctx->results[i] = r;
            ctx->values[i]  = v;
            ctx->count++;
            if (ctx->size == ctx->count) {
                ctx->promise.setValue(ctx->results, ctx->values);
            }
        }, pool);
    }

    return ctx->promise.getFuture();
}

template<typename Result, typename Type>
static Future<Result, Type> makeReadyFuture(Result&& result, Type&& value) {
    Promise<Result, Type> promise;
    promise.setValue(result, value);
    return promise.getFuture();
}

template<typename Result, typename Type>
static Future<Result, Type> makeReadyFuture() {
    static Result DEFAULT_RESULT;
    static Type DEFAULT_VALUE;

    Promise<Result, Type> promise;
    promise.setValue(DEFAULT_RESULT, DEFAULT_VALUE);
    return promise.getFuture();
}

}

#endif /* LIB_FUTURE_H_ */
