
#pragma once

#include "iostream"
#include "fiber/fiber_tbox.hpp"

const int DefaultSize = 10;
const int MaxSize = 100;

template<class Type>
class ObjectPool {
public:
    ObjectPool(int size = DefaultSize, int max_size = MaxSize)
    : pool_(), size_(size), max_size_(max_size) {
        for (int i = 0; i < size; i++) {
            auto t = new Type;
            pool_.push(t);
        }
    }

    Type* Get() {
        auto t = pool_.pop(0);
        if (t == nullptr) {
            return new Type;
        }
        return t;
    }

    void Release(Type* t) {
        if (pool_.size() > max_size_) {
            delete t;
            return;
        }
        pool_.push(t);
    }

private:
    acl::fiber_tbox<Type> pool_;
    int size_;
    int max_size_;
};
