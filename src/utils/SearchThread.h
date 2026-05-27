#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stop_token>
#include <thread>
#include <utility>
#include <variant>

#if defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

class SearchThread {
public:
    SearchThread() = default;

    ~SearchThread() {
        join();
    }

    SearchThread(const SearchThread&) = delete;
    SearchThread& operator=(const SearchThread&) = delete;

    SearchThread(SearchThread&& other) noexcept
        : handle_(std::exchange(other.handle_, std::monostate{})) {}

    SearchThread& operator=(SearchThread&& other) noexcept {
        if (this != &other) {
            join();
            handle_ = std::exchange(other.handle_, std::monostate{});
        }
        return *this;
    }

    template <typename Callable>
    [[nodiscard]] int start(Callable&& callable, std::size_t stackBytes = 0,
                            std::stop_token stopToken = {}) {
        join();

        auto task = std::make_shared<std::decay_t<Callable>>(std::forward<Callable>(callable));
        auto runner =
            std::make_shared<std::function<void(std::stop_token)>>([task](std::stop_token token) {
                if constexpr (requires { (*task)(token); }) {
                    (*task)(token);
                } else {
                    (void)token;
                    (*task)();
                }
            });

#if defined(__APPLE__) || defined(__linux__)
        if (stackBytes > 0) {
            pthread_attr_t attr{};
            if (pthread_attr_init(&attr) == 0) {
                const int stackResult = pthread_attr_setstacksize(&attr, stackBytes);
                if (stackResult == 0) {
                    auto handle = std::make_shared<PthreadHandle>();
                    handle->runner = runner;
                    handle->stopToken = stopToken;
                    const int createResult = pthread_create(
                        &handle->thread, &attr,
                        +[](void* arg) -> void* {
                            auto* ctx = static_cast<PthreadHandle*>(arg);
                            (*ctx->runner)(ctx->stopToken);
                            return nullptr;
                        },
                        handle.get());
                    pthread_attr_destroy(&attr);

                    if (createResult == 0) {
                        handle->joinable = true;
                        handle_ = std::move(handle);
                        return 0;
                    }
                } else {
                    pthread_attr_destroy(&attr);
                }
            }
        }
#endif

        return startWithJthread(*runner, stopToken);
    }

    void join() {
        if (auto* jthread = std::get_if<std::jthread>(&handle_)) {
            if (jthread->joinable()) {
                jthread->join();
            }
        } else if (auto* pthreadHandle = std::get_if<PthreadHandlePtr>(&handle_)) {
            if (*pthreadHandle && (*pthreadHandle)->joinable) {
                pthread_join((*pthreadHandle)->thread, nullptr);
                (*pthreadHandle)->joinable = false;
            }
            *pthreadHandle = nullptr;
        }
        handle_ = std::monostate{};
    }

    [[nodiscard]] bool joinable() const {
        if (const auto* jthread = std::get_if<std::jthread>(&handle_)) {
            return jthread->joinable();
        }
        if (const auto* pthreadHandle = std::get_if<PthreadHandlePtr>(&handle_)) {
            return *pthreadHandle && (*pthreadHandle)->joinable;
        }
        return false;
    }

private:
    struct PthreadHandle {
        pthread_t thread{};
        bool joinable = false;
        std::shared_ptr<std::function<void(std::stop_token)>> runner;
        std::stop_token stopToken{};
    };

    using PthreadHandlePtr = std::shared_ptr<PthreadHandle>;

    [[nodiscard]] int startWithJthread(const std::function<void(std::stop_token)>& runner,
                                       std::stop_token stopToken) {
        handle_ = std::jthread([runner, stopToken](std::stop_token threadToken) {
            if (threadToken.stop_requested() || stopToken.stop_requested()) {
                return;
            }
            runner(threadToken.stop_requested() ? threadToken : stopToken);
        });
        return 0;
    }

    std::variant<std::monostate, std::jthread, PthreadHandlePtr> handle_;
};

class SearchThreadAttr {
public:
    [[nodiscard]] int setStackSize(std::size_t stackBytes) {
        stackBytes_ = stackBytes;
        return 0;
    }

    [[nodiscard]] std::size_t stackSize() const {
        return stackBytes_;
    }

private:
    std::size_t stackBytes_ = 0;
};
