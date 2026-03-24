#pragma once

#include <string>
#include <vector>

namespace BenchmarkArgs {

int parsePositiveIntArg(const std::vector<std::string>& args, const std::string& key, int fallback);
int parsePositiveIntArg(int argc, char** argv, const std::string& key, int fallback);
std::string parseStringArg(const std::vector<std::string>& args, const std::string& key,
                           const std::string& fallback);
std::vector<std::string> parseCsvArg(const std::vector<std::string>& args, const std::string& key);
std::vector<int> parsePositiveIntListArg(const std::vector<std::string>& args,
                                         const std::string& key);

} // namespace BenchmarkArgs
