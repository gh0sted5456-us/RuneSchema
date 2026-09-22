#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=3)throw std::runtime_error("Adapter and constructor source required");
    const auto read=[](const char* path) {
        std::ifstream file(path);if(!file)throw std::runtime_error("Source unavailable");
        return std::string(std::istreambuf_iterator<char>(file),{});
    };
    const auto adapter=read(argv[1]),constructor=read(argv[2]);
    const auto require=[](bool good){if(!good)throw std::runtime_error("Quest restart ownership regression");};
    require(constructor.find("RF_Public | RF_Standalone | RF_Transactional")!=std::string::npos);
    const auto start=adapter.find("void ChangeRestartState(");
    const auto guard=adapter.substr(start,adapter.find("auto* array=",start)-start);
    require(guard.find("!authority.Result<bool>()")!=std::string::npos);
    require(guard.find("GetOuterPrivate()->GetPathName()!=TEXT(\"/Engine/Transient\")")!=std::string::npos);
    require(guard.find("RuneSchema_Quest_")!=std::string::npos);
    require(guard.find("HasAnyFlags(RF_Transient)")==std::string::npos);
    require(adapter.find("Quest restart identity is duplicated")!=std::string::npos);
    require(adapter.find("prepare();")!=std::string::npos);
    require(adapter.find("SetGiven(true);")!=std::string::npos);
    require(adapter.find("if(name==\"Given\")NotifyRecovery();")!=std::string::npos);
}
