
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Production StartupTrace source required");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("Source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto start=source.find("void Mark("),setting=source.find("const bool detailed=PSConfig::Get()->IsDebugLoggingEnabled();",start);
    const auto record=source.find("Timeline.Record(",start),guard=source.find("if(!detailed)return;",start);
    const auto open=source.find("Stream.open(",start),end=source.find("void Fatal(",start);
    const auto folders=source.find("std::filesystem::create_directories(",start);
    if(start==source.npos||setting==source.npos||record==source.npos||guard==source.npos||open==source.npos||end==source.npos||folders==source.npos
        ||!(start<setting&&setting<record&&record<guard&&guard<folders&&folders<open&&open<end))
        throw std::runtime_error("Startup debug trace must respect the actual logging opt-in before writing");
    const auto snapshot=source.find("nlohmann::json Snapshot()");
    if(snapshot==source.npos||snapshot>end)throw std::runtime_error("Read-only startup statistics snapshot missing");
    const auto snapshotBody=source.substr(snapshot,end-snapshot);
    if(snapshotBody.find("Timeline.Rows()")==source.npos||snapshotBody.find("Stream.")!=source.npos||snapshotBody.find("create_directories")!=source.npos)
        throw std::runtime_error("Normal-mode startup statistics must remain bounded memory-only reads");
    if(source.substr(end).find("startup-failure.log")==source.npos||source.substr(end).find("OutputDebugStringA")==source.npos)
        throw std::runtime_error("Fatal startup reporting must remain available independently of debug logging");
}
