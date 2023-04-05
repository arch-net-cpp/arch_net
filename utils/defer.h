#pragma once
#include "iostream"

template<typename T>
class Defer
{
public:
    Defer(T fn) : m_func(fn) {}
    ~Defer() { m_func(); }
    void operator=(const Defer<T>&) = delete;
    operator bool () { return true; }   // if (DEFER(...)) { ... }

private:
    T m_func;
};

template<typename T>
Defer<T> make_defer(T func) { return Defer<T>(func); }

#define __INLINE__ __attribute__((always_inline))
#define __FORCE_INLINE__ __INLINE__ inline

#define _CONCAT_(a, b) a##b
#define _CONCAT(a, b) _CONCAT_(a, b)

#define defer(func) auto _CONCAT(__defer__, __LINE__) = \
    make_defer([&]() __INLINE__ { func; })

