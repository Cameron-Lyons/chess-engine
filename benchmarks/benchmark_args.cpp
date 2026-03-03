#include "benchmark_args.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace BenchmarkArgs {

int parsePositiveIntArg(const std::vector<std::string>& args, const std::string& key, int fallback) {
    const std::string prefix = key + "=";
    for (const std::string& arg : args) {
        if (!arg.starts_with(prefix)) {
            continue;
        }

        const std::string value = arg.substr(prefix.size());
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
