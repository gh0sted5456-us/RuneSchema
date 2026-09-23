#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string Read(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) std::exit(2);
    std::ostringstream value;
    value << file.rdbuf();
    return value.str();
}

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    Check(argc == 5, "four source inputs supplied");
    const auto manager = Read(argv[1]);
    const auto loader = Read(argv[2]);
    const auto loadOrder = Read(argv[3]);
    const auto host = Read(argv[4]);
    Check(manager.find("ResolveUObjectVirtual") != std::string::npos, "vtable provider exists");
    Check(manager.find("UObject::VTableLayoutMap") != std::string::npos, "UE4SS vtable metadata used");
    Check(manager.find("VirtualQuery") != std::string::npos, "resolved target memory validated");
    Check(manager.find("live-vtable:") != std::string::npos, "provider provenance retained");
    Check(loader.find("Serialize__Ref_FArchive") != std::string::npos, "FArchive overload selected");
    Check(loader.find("if (DatatableSerializeCallbacks.empty())") != std::string::npos,
        "late Unreal-ready binding retry exists");
    Check(loader.find("core startup will continue through GameInstance") != std::string::npos,
        "missing early binding is feature-scoped");
    Check(loader.find("Unable to initialize RuneSchema core, signature for UDataTable::Serialize") == std::string::npos,
        "obsolete fatal diagnostic removed");
    Check(loadOrder.find("create_directories(path.parent_path()") != std::string::npos,
        "missing mods directory is created before load-order write");
    Check(host.find("runeschema.bindings") != std::string::npos, "binding discovery service registered");
    Check(host.find("binding.resolve") != std::string::npos, "binding capability registered");
    Check(host.find("ResolveBinding") != std::string::npos, "host binding resolver exported");
    std::cout << "Native binding resolution contract passed.\n";
}
