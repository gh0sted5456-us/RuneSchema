#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Loader base source is required");
    std::ifstream file(argv[1],std::ios::binary);
    if(!file)throw std::runtime_error("Loader base source is unavailable");
    const std::string source{std::istreambuf_iterator<char>(file),{}};
    if(source.find("!CanInitialize(engineLifecyclePhase)")==std::string::npos)
        throw std::runtime_error("Deferred lifecycle sections can still mark a mod partial");
    if(source.find("!PS::PSConfig::Get()->IsLoaderEnabled(m_modFolderType)")==std::string::npos)
        throw std::runtime_error("Disabled loader sections can still mark a mod partial");
}
