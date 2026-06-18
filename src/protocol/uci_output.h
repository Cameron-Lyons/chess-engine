#pragma once

#include <cstdio>
#include <format>
#include <mutex>
#include <print>

namespace uci::output {

inline FILE*& stream() {
    static FILE* out = stdout;
    return out;
}

inline std::mutex& writeMutex() {
    static std::mutex mutex;
    return mutex;
}

template <class... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    const std::lock_guard lock(writeMutex());
    std::print(stream(), fmt, std::forward<Args>(args)...);
}

template <class... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    const std::lock_guard lock(writeMutex());
    std::println(stream(), fmt, std::forward<Args>(args)...);
}

} // namespace uci::output
