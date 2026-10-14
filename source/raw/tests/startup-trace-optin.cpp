
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Production StartupTrace source required");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("Source unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto start=source.find("void Mark("),guard=source.find("if (!PSConfig::Get()->IsDebugLoggingEnabled()) return;",start);
    const auto open=source.find("Stream.open(",start),end=source.find("void Fatal(",start);
    if(start==source.npos||guard==source.npos||open==source.npos||end==source.npos||!(start<guard&&guard<open&&open<end))
        throw std::runtime_error("Startup debug trace must respect the actual logging opt-in before writing");
    if(source.substr(end).find("startup-failure.log")==source.npos||source.substr(end).find("OutputDebugStringA")==source.npos)
        throw std::runtime_error("Fatal startup reporting must remain available independently of debug logging");
}
