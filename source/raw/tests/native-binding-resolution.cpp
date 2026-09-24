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
    Check(argc == 8, "seven source inputs supplied");
    const auto manager = Read(argv[1]);
    const auto loader = Read(argv[2]);
    const auto loadOrder = Read(argv[3]);
    const auto host = Read(argv[4]);
    const auto storefront = Read(argv[5]);
    const auto offsets = Read(argv[6]);
    const auto entrypoint = Read(argv[7]);
    Check(manager.find("ResolveUObjectVirtual") != std::string::npos, "vtable provider exists");
    Check(manager.find("UObject::VTableLayoutMap") != std::string::npos, "UE4SS vtable metadata used");
    Check(manager.find("VirtualQuery") != std::string::npos, "resolved target memory validated");
    Check(manager.find("live-vtable:") != std::string::npos, "provider provenance retained");
    Check(loader.find("Serialize__Ref_FArchive") != std::string::npos, "FArchive overload selected");
    Check(loader.find("if (DatatableSerializeCallbacks.empty())") != std::string::npos,
        "late Unreal-ready binding retry exists");
    Check(loader.find("normal GameInstance loading continues") != std::string::npos,
        "missing early binding is feature-scoped");
    Check(loader.find("Unable to initialize RuneSchema core, signature for UDataTable::Serialize") == std::string::npos,
        "obsolete fatal diagnostic removed");
    Check(loadOrder.find("create_directories(path.parent_path()") != std::string::npos,
        "missing mods directory is created before load-order write");
    Check(host.find("runeschema.bindings") != std::string::npos, "binding discovery service registered");
    Check(host.find("binding.resolve") != std::string::npos, "binding capability registered");
    Check(host.find("ResolveBinding") != std::string::npos, "host binding resolver exported");
    Check(manager.find("CurrentNativeLane") != std::string::npos,
        "embedded executable signatures are selected by the active native lane");
    Check(manager.find("[BINDING:{}][UNAVAILABLE]") != std::string::npos,
        "a missing optional executable pattern is reported as a scoped warning");
    Check(storefront.find("NativeLane::SteamNative") != std::string::npos
        && storefront.find("NativeLane::GamePassNative") != std::string::npos
        && storefront.find("NativeLane::SharedOnly") != std::string::npos,
        "storefront selects one explicit native lane");
    Check(storefront.find("lane == NativeLane::SteamNative || lane == NativeLane::GamePassNative") != std::string::npos,
        "embedded AOBs are restricted to explicit storefront lanes");
    Check(manager.find("SteamSignatures") != std::string::npos
        && manager.find("GamePassSignatures") != std::string::npos,
        "Steam and WinGDK signatures are selected from isolated maps");
    for (const auto* oldGamePassFatal : {
            "Unable to initialize RuneSchema core, signature for UDataTable::Serialize is outdated",
            "Failed to find signature for FFieldClass::GetNameToFieldClassMap",
            "Failed to find signature for GetObjectsOfClass",
            "Failed to find signature for FName::ToString_Wchar"})
        Check(loader.find(oldGamePassFatal) == std::string::npos
                && manager.find(oldGamePassFatal) == std::string::npos,
            "0.7.5 Game Pass AOB failure cannot return");
    Check(offsets.find("if (FNameConstructorAddress)") != std::string::npos
        && offsets.find("if (FNameToStringAddress)") != std::string::npos,
        "missing storefront override preserves UE4SS host bindings");
    Check(entrypoint.find("Native binding lane:") != std::string::npos,
        "selected lane is announced for diagnostics");
    std::cout << "Native binding resolution contract passed.\n";
}
