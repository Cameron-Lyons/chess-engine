#pragma once

#include <string>
#include <vector>

namespace BenchmarkArgs {

int parsePositiveIntArg(const std::vector<std::string>& args, const std::string& key, int fallback);
int parsePositiveIntArg(int argc, char** argv, const std::string& key, int fallback);

} // namespace BenchmarkArgs
