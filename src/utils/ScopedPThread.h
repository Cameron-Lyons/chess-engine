#pragma once

#include <cstddef>
#include <pthread.h>

class ScopedPThread {
public:
    using StartRoutine = void* (*)(void*);

    ScopedPThread() = default;
    ~ScopedPThread() {
        join();
    }

    ScopedPThread(const ScopedPThread&) = delete;
    auto operator=(const ScopedPThread&) -> ScopedPThread& = delete;

    ScopedPThread(ScopedPThread&& other) noexcept
        : thread(other.thread), joinableThread(other.joinableThread) {
        other.joinableThread = false;
    }

    auto operator=(ScopedPThread&& other) noexcept -> ScopedPThread& {
        if (this == &other) {
            return *this;
        }

        join();
        thread = other.thread;
        joinableThread = other.joinableThread;
        other.joinableThread = false;
        return *this;
    }

    [[nodiscard]] int start(const pthread_attr_t* attr, StartRoutine routine, void* arg) {
        join();

        pthread_t nextThread{};
        const int result = pthread_create(&nextThread, attr, routine, arg);
        if (result == 0) {
            thread = nextThread;
            joinableThread = true;
        }
        return result;
    }

    void join() noexcept {
        if (!joinableThread) {
            return;
        }

        pthread_join(thread, nullptr);
        joinableThread = false;
    }

    [[nodiscard]] bool joinable() const noexcept {
        return joinableThread;
    }

private:
    pthread_t thread{};
    bool joinableThread = false;
};

class ScopedPThreadAttr {
public:
    ScopedPThreadAttr() noexcept : initResult(pthread_attr_init(&attr)) {}

    ~ScopedPThreadAttr() {
        if (initResult == 0) {
            pthread_attr_destroy(&attr);
        }
    }

    ScopedPThreadAttr(const ScopedPThreadAttr&) = delete;
    auto operator=(const ScopedPThreadAttr&) -> ScopedPThreadAttr& = delete;
    ScopedPThreadAttr(ScopedPThreadAttr&&) = delete;
    auto operator=(ScopedPThreadAttr&&) -> ScopedPThreadAttr& = delete;

    [[nodiscard]] int initStatus() const noexcept {
        return initResult;
    }

    [[nodiscard]] int setStackSize(std::size_t stackSize) noexcept {
        if (initResult != 0) {
            return initResult;
        }
        return pthread_attr_setstacksize(&attr, stackSize);
    }

    [[nodiscard]] const pthread_attr_t* get() const noexcept {
        return initResult == 0 ? &attr : nullptr;
    }

private:
    pthread_attr_t attr{};
    int initResult = 0;
};
