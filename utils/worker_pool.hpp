#pragma once
#include <utility>

#include "fiber.h"
#include "object_pool.hpp"
#include "singleton.h"
#include "thread"
#include "random"
#include <atomic>
#include <climits>

static const int WORKERPOOL_DEFAULT_THREAD_NUM = 4;

enum WorkStrategy {
    RR,
    Random,
    LoadBalance,
    HashConsistent
};

using ChanType = acl::fiber_tbox<FiberCtx>;

class WorkerPool {
public:
    explicit WorkerPool(int thread_num = WORKERPOOL_DEFAULT_THREAD_NUM, WorkStrategy strategy = LoadBalance)
    : strategy_(strategy), thread_num_(thread_num), gen_(std::random_device{}()), dis_(0, thread_num-1){};

    virtual void addTask(std::function<void()> func, int hash_seed) = 0;

    virtual void addTask(std::function<void()> func) = 0;

    virtual ~WorkerPool(){};

    int get_next_index(int hash_seed) {
        size_t max_size;
        int idx = 0;
        switch (strategy_) {
        case RR:
            return next_index_.fetch_add(1) % thread_num_;
        case Random:
            return dis_(gen_);
        case LoadBalance:
            max_size = thread_chans_[0]->size();
            for (int i = 1; i < thread_num_; i++) {
                if (thread_chans_[i]->size() < max_size) {
                    idx = i;
                }
            }
            return idx;
        case HashConsistent:
            return std::hash<int>()(hash_seed) % thread_num_;
        }
        return 0;
    }

protected:
    ObjectPool<FiberCtx> ctx_pool_{};
    std::vector<std::unique_ptr<std::thread>> threads_{};
    std::vector<std::unique_ptr<ChanType>> thread_chans_{};
    acl::wait_group wg_{};

    WorkStrategy strategy_;
    int thread_num_;
    std::mt19937 gen_;
    std::uniform_int_distribution<int> dis_;
    std::atomic<long> next_index_{0};
};

class CPUWorkerPool : public WorkerPool {
public:
    using ChanType = acl::fiber_tbox<FiberCtx>;

    explicit CPUWorkerPool(int thread_num = WORKERPOOL_DEFAULT_THREAD_NUM, WorkStrategy strategy = RR )
    : WorkerPool(thread_num, strategy) {

        wg_.add(thread_num);
        for (int i = 0; i <  thread_num; i++) {
            thread_chans_.emplace_back(std::make_unique<ChanType>(false));
            threads_.emplace_back(std::make_unique<std::thread>([this, idx = i]{
                while (true) {
                    auto ctx = thread_chans_[idx]->pop();
                    if (!ctx) break;
                    // try exception
                    ctx->fn_();
                    ctx_pool_.Release(ctx);
                }
                wg_.done();
            }));
        }
    }
    void addTask(std::function<void ()> func, int hash_seed=0) override {
        auto idx = get_next_index(hash_seed);
        auto ctx = ctx_pool_.Get();
        ctx->fn_ = std::move(func);
        thread_chans_[idx]->push(ctx);
    }

    void addTask(std::function<void ()> func) override {
        addTask(std::move(func), 0);
    }

    ~CPUWorkerPool() override {
        for (int i = 0; i < thread_num_; i++) {
            thread_chans_[i]->push(nullptr);
        }
        wg_.wait();
        for (int i = 0; i < thread_num_; i++) {
            threads_[i]->join();
        }
    }
};


class IOWorkerPool : public WorkerPool {
public:
    using ChanType = acl::fiber_tbox<FiberCtx>;

    explicit IOWorkerPool(int thread_num = WORKERPOOL_DEFAULT_THREAD_NUM, WorkStrategy strategy = Random)
        : WorkerPool(thread_num, strategy) {

        wg_.add(thread_num);
        for (int i = 0; i <  thread_num; i++) {
            thread_chans_.emplace_back(std::make_unique<ChanType>(false));
            threads_.emplace_back(std::make_unique<std::thread>([this, idx = i]{
                go[this, idx](){
                    while (true) {
                        auto ctx = thread_chans_[idx]->pop();
                        if (!ctx) break;
                        // try exception
                        go[this, ctx=ctx, idx=idx]() {
                            auto fn = std::move(ctx->fn_);
                            ctx_pool_.Release(ctx);
                            fn();
                            return;
                            while (true) {
                                auto new_ctx = thread_chans_[idx]->pop(0);
                                if (!new_ctx) {
                                    break;
                                }
                                fn = std::move(new_ctx->fn_);
                                ctx_pool_.Release(new_ctx);
                                fn();
                            }
                        };
                    }
                    acl::fiber::schedule_stop();
                };
                acl::fiber::schedule_with(acl::FIBER_EVENT_T_KERNEL);
                wg_.done();
            }));
        }
    }

    void addTask(std::function<void()> func, int hash_seed) override {
        auto idx = get_next_index(hash_seed);

        auto ctx = ctx_pool_.Get();
        ctx->fn_ = std::move(func);
        thread_chans_[idx]->push(ctx);
    }

    void addTask(std::function<void ()> func) override {
        addTask(std::move(func), 0);
    }

    ~IOWorkerPool() override {
        for (int i = 0; i < thread_num_; i++) {
            thread_chans_[i]->push(nullptr);
        }
        wg_.wait();
        for (int i = 0; i < thread_num_; i++) {
            threads_[i]->join();
        }
    }
};

class GlobalIOWorkerPool : public Singleton<GlobalIOWorkerPool> {
public:
    GlobalIOWorkerPool() {
        pool_ = std::make_unique<IOWorkerPool>(WORKERPOOL_DEFAULT_THREAD_NUM);
    }
    static WorkerPool* GetPool() {
        auto& instance = GlobalIOWorkerPool::getInstance();
        return instance.pool_.get();
    }
private:
    std::unique_ptr<IOWorkerPool> pool_;
};

class ConsistentIOWorker {
public:
    explicit ConsistentIOWorker() {
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<int> dis(0, INT_MAX);
        seed_ = dis(gen);
    }

    void addTask(std::function<void ()> func) {
        GlobalIOWorkerPool::GetPool()->addTask(std::move(func), seed_);
    }

private:
    int seed_;
};