#include "benchmark_args.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace BenchmarkArgs {

std::string parseStringArg(const std::vector<std::string>& args, const std::string& key,
                           const std::string& fallback) {
    const std::string prefix = key + "=";
    for (const std::string& arg : args) {
        if (!arg.starts_with(prefix)) {
            continue;
        }
        return arg.substr(prefix.size());
    }
    return fallback;
}

std::vector<std::string> parseCsvArg(const std::vector<std::string>& args, const std::string& key) {
    const std::string raw = parseStringArg(args, key, "");
    if (raw.empty()) {
        return {};
    }

    std::vector<std::string> values;
    std::stringstream stream(raw);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            values.push_back(item);
        }
    }
    return values;
}

std::vector<int> parsePositiveIntListArg(const std::vector<std::string>& args,
                                         const std::string& key) {
    std::vector<int> values;
    for (const std::string& item : parseCsvArg(args, key)) {
        char* end = nullptr;
        errno = 0;
        const long parsed = std::strtol(item.c_str(), &end, 10);
        if (errno == 0 && end != item.c_str() && *end == '\0' && parsed > 0 &&
            parsed <= std::numeric_limits<int>::max()) {
            values.push_back(static_cast<int>(parsed));
        }
    }
    return values;
}

int parsePositiveIntArg(const std::vector<std::string>& args, const std::string& key,
                        int fallback) {
    for (const std::string& value : parseCsvArg(args, key)) {
        char* end = nullptr;
        errno = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (errno == 0 && end != value.c_str() && *end == '\0' && parsed > 0 &&
            parsed <= std::numeric_limits<int>::max()) {
            return static_cast<int>(parsed);
        }
    }
    return fallback;
}

int parsePositiveIntArg(int argc, char** argv, const std::string& key, int fallback) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return parsePositiveIntArg(args, key, fallback);
}

} // namespace BenchmarkArgs
