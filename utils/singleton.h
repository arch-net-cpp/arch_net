#pragma once

// define Singleton
template <typename T>
class Singleton {
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
    // delete copy and move constructors and assign operators
    Singleton(Singleton const&) = delete;             // Copy construct
    Singleton(Singleton&&) = delete;                  // Move construct
    Singleton& operator=(Singleton const&) = delete;  // Copy assign
    Singleton& operator=(Singleton&&) = delete;       // Move assign
protected:
    Singleton() {}
    ~Singleton() {}
};

// define Singleton
template <typename T>
class ThreadLocalSingleton {
public:
    static T& getInstance() {
        static thread_local T instance;
        return instance;
    }
    // delete copy and move constructors and assign operators
    ThreadLocalSingleton(ThreadLocalSingleton const&) = delete;             // Copy construct
    ThreadLocalSingleton(ThreadLocalSingleton&&) = delete;                  // Move construct
    ThreadLocalSingleton& operator=(ThreadLocalSingleton const&) = delete;  // Copy assign
    ThreadLocalSingleton& operator=(ThreadLocalSingleton&&) = delete;       // Move assign
protected:
    ThreadLocalSingleton() {}
    ~ThreadLocalSingleton() {}
};