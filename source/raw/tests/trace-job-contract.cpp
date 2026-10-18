#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "nlohmann/json.hpp"

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("cannot read "+path.string());
    std::ostringstream text;text<<file.rdbuf();return text.str();
}

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    const auto loader=Read(argv[1]);
    const auto config=Read(argv[2]);

    assert(loader.find("SettingsDirectory()/\"jobs\"")!=std::string::npos);
    assert(loader.find("RuneSchema.TraceJobResult.v1")!=std::string::npos);
    assert(loader.find("if(job.ConsoleEvents)PS::Log")!=std::string::npos);
    assert(loader.find("characterEditorPreset")==std::string::npos);
    assert(loader.find("add(\"character-editor\"")==std::string::npos);
    assert(config.find("characterEditorPreset")==std::string::npos);

    std::cout<<"Trace jobs are JSON-owned and file-exported without default console event spam.\n";
}
