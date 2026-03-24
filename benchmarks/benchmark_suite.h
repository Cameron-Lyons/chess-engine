#pragma once

#include <memory>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

namespace BenchmarkSuite {

enum class OutputFormat : unsigned char { TEXT, JSON };

struct PositionCase {
    std::string id;
    std::string name;
    std::string fen;
    std::vector<std::string> tags;
};

std::vector<PositionCase> defaultCorpus();
std::vector<PositionCase> selectPositions(const std::vector<std::string>& args);
OutputFormat parseOutputFormat(const std::vector<std::string>& args);
std::string outputFormatName(OutputFormat format);
std::string jsonEscape(const std::string& value);
std::string joinTags(const std::vector<std::string>& tags, const std::string& separator);
void initializeEngineState();

class ScopedStdoutSilencer {
public:
    explicit ScopedStdoutSilencer(bool enabled);
    ~ScopedStdoutSilencer();

private:
    std::streambuf* originalBuffer = nullptr;
    std::unique_ptr<std::ostringstream> sinkStorage;
    bool enabled = false;
};

} // namespace BenchmarkSuite
