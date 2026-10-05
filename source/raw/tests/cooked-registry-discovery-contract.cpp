#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error(std::string("Cannot read ")+path);
    return {std::istreambuf_iterator<char>(file),{}};
}

int main(int argc,char** argv) {
    if(argc!=5)throw std::runtime_error("Expected registry loader, main loader, owner catalog and bridge guide");
    const auto registry=Read(argv[1]),main=Read(argv[2]),owners=Read(argv[3]),guide=Read(argv[4]);
    const auto require=[](bool value,const char* message) {
        if(!value)throw std::runtime_error(std::string("Cooked registry discovery regression: ")+message);
    };
    require(main.find("MountedModRegistryOwners::Reset")!=std::string::npos
        &&main.find("MountedModRegistryOwners::Remember")!=std::string::npos,
        "enabled ordered mod folders are not captured during PAK discovery");
    require(owners.find("value.size()<=64")!=std::string::npos
        &&owners.find("std::isalnum(c)||c=='_'")!=std::string::npos,
        "conventional owner names are not narrowly bounded");
    require(registry.find("/Game/Mods/")!=std::string::npos
        &&registry.find("/Registry/")!=std::string::npos
        &&registry.find("DA_RuneSchemaRegistry_")!=std::string::npos,
        "the deterministic direct-load convention is missing");
    require(registry.find("owner!=expectedOwner")!=std::string::npos
        &&registry.find("owner does not match its enabled mod folder")!=std::string::npos,
        "direct-loaded declarations are not bound to their enabled mod owner");
    require(registry.find("if(!object)continue")!=std::string::npos,
        "mods without a conventional declaration are treated as errors");
    require(guide.find("Asset Registry metadata")!=std::string::npos
        &&guide.find("/Game/Mods/<ModFolder>/Registry/DA_RuneSchemaRegistry_<ModFolder>")!=std::string::npos,
        "the metadata-independent convention is undocumented");
}
