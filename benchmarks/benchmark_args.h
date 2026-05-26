#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace BenchmarkArgs {

int parsePositiveIntArg(std::span<const std::string> args, std::string_view key, int fallback);
int parsePositiveIntArg(int argc, char** argv, std::string_view key, int fallback);
std::string parseStringArg(std::span<const std::string> args, std::string_view key,
                           std::string_view fallback);
std::vector<std::string> parseCsvArg(std::span<const std::string> args, std::string_view key);
std::vector<int> parsePositiveIntListArg(std::span<const std::string> args, std::string_view key);

} // namespace BenchmarkArgs
