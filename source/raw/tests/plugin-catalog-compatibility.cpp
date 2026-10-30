#include "Runtime/PluginCatalog.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void Write(const fs::path& path, const std::string& value = {})
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    Check(static_cast<bool>(stream), "test file opens");
    stream << value;
}

static void AddPakTriplet(const fs::path& plugin)
{
    const auto package = plugin / "paks" / "CompatibilityContent";
    Write(package / "CompatibilityContent.pak");
    Write(package / "CompatibilityContent.ucas");
    Write(package / "CompatibilityContent.utoc");
}

int main() try
{
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("runeschema-plugin-contract-" + std::to_string(nonce));
    struct Cleanup {
        fs::path Path;
        ~Cleanup() { std::error_code error; fs::remove_all(Path, error); }
    } cleanup{root};

    const auto alpha = root / "Alpha";
    const auto beta = root / "Beta";
    Write(alpha / "plugin.json", R"({
        "SchemaVersion": 1,
        "Id": "Alpha",
        "Name": "Alpha",
        "Version": "99.0.0",
        "ApiVersion": 999,
        "BuiltForRuneSchema": "0.1.0",
        "EntryPoint": "missing-alpha.dll",
        "MountPaks": false,
        "Dependencies": { "Beta": "1.0.0" }
    })");
    Write(beta / "plugin.json", R"({
        "SchemaVersion": 1,
        "Id": "Beta",
        "Name": "Beta",
        "Version": "2.0.0",
        "ApiVersion": 1,
        "Dependencies": { "Alpha": "1.0.0", "NotInstalled": "*" }
    })");
    AddPakTriplet(alpha);
    AddPakTriplet(beta);
    Write(alpha / "paks" / "__folder_managed_by_vortex");
    Write(alpha / "paks" / "README.txt", "ignored");
    Write(alpha / "paks" / "CompatibilityContent" / "meta.ini", "ignored");
    Write(root / "random-nexus-file.txt", "ignored");
    Write(root / "UnrelatedFolder" / "readme.md", "ignored");
    Write(root / "plugins.txt", "Alpha:1\nBeta:1\n");

    std::vector<std::string> diagnostics;
    const auto plugins = PS::PluginCatalog::Discover(root, &diagnostics);
    Check(plugins.size() == 2, "mismatched and cyclic plugins remain discoverable");
    Check(!plugins[0].EntryPoint.empty() || !plugins[1].EntryPoint.empty(),
        "missing optional native entry point remains represented");
    Check(diagnostics.empty() || std::none_of(diagnostics.begin(), diagnostics.end(), [](const auto& line) {
        return line.find("vortex") != std::string::npos || line.find("README.txt") != std::string::npos
            || line.find("random-nexus-file") != std::string::npos || line.find("UnrelatedFolder") != std::string::npos;
    }), "mod-manager debris is silently ignored");
    for (const auto& plugin : plugins) {
        const auto packs=PS::PluginCatalog::PakDirectories(plugin);
        if(plugin.Id=="Alpha") Check(packs.empty(),"MountPaks false ignores stale native-plugin package content");
        else Check(packs.size()==1,"complete PAK triplet remains independently mountable");
    }
    {
        PS::PluginCatalog::Plugin linked;linked.Id="VortexLinked";linked.PaksRoot=root/"vortex"/"paks";
        const auto stage=root/"staging",package=linked.PaksRoot/"RegistryBridge";
        fs::create_directories(package);
        for(const auto& name:{"RegistryBridge_P.PAK","registrybridge_p.ucas","RegistryBridge_P.UTOC"}){
            Write(stage/name,"fixture");fs::create_hard_link(stage/name,package/name);
        }
        Write(linked.PaksRoot/"metadata-only"/"__folder_managed_by_vortex","ignored");
        Check(PS::PluginCatalog::PakDirectories(linked).size()==1,"mixed-case hardlinked triplet accepts Windows filename semantics");
        fs::remove(package/"registrybridge_p.ucas");bool rejected=false;
        try{PS::PluginCatalog::PakDirectories(linked);}catch(const std::exception& error){
            const std::string message=error.what();rejected=message.find("missing")!=message.npos&&message.find(".ucas")!=message.npos&&message.find("found")!=message.npos;
        }
        Check(rejected,"missing hardlink is rejected with the exact absent part and observed filenames");
        fs::create_hard_link(stage/"registrybridge_p.ucas",package/"registrybridge_p.ucas");
        Check(PS::PluginCatalog::PakDirectories(linked).size()==1,"redeployed complete triplet recovers without renaming staging files");
    }

    bool missingDependency = false;
    bool cycle = false;
    for (const auto& diagnostic : diagnostics) {
        missingDependency = missingDependency || diagnostic.find("dependency NotInstalled is unavailable") != std::string::npos;
        cycle = cycle || diagnostic.find("dependency cycle detected") != std::string::npos;
    }
    Check(missingDependency, "missing dependency is diagnosed without suppressing discovery");
    Check(cycle, "dependency cycle falls back to deterministic best-effort order");

    // plugins.txt is the explicit preferred order. Dependencies may move a
    // provider earlier, but otherwise the file order must be preserved.
    const auto orderRoot=root/"order";
    for(const auto* id:{"AlphaOrder","BetaOrder","GammaOrder"}) {
        const auto folder=orderRoot/id;
        Write(folder/"plugin.json",
            std::string("{\"SchemaVersion\":1,\"Id\":\"")+id+"\",\"Version\":\"1.0.0\"}");
    }
    Write(orderRoot/"plugins.txt","GammaOrder:1\nAlphaOrder:1\nBetaOrder:1\n");
    const auto ordered=PS::PluginCatalog::Discover(orderRoot);
    Check(ordered.size()==3,"ordered plugin fixture is complete");
    Check(ordered[0].Id=="GammaOrder"&&ordered[1].Id=="AlphaOrder"&&ordered[2].Id=="BetaOrder",
        "plugins.txt top-to-bottom order is preserved when dependencies do not constrain it");

    const auto dependencyRoot=root/"dependency-order";
    Write(dependencyRoot/"Consumer"/"plugin.json",
        R"({"SchemaVersion":1,"Id":"Consumer","Version":"1.0.0","Dependencies":{"Provider":"1.0.0"}})");
    Write(dependencyRoot/"Provider"/"plugin.json",
        R"({"SchemaVersion":1,"Id":"Provider","Version":"1.0.0"})");
    Write(dependencyRoot/"plugins.txt","Consumer:1\nProvider:1\n");
    const auto dependencyOrdered=PS::PluginCatalog::Discover(dependencyRoot);
    Check(dependencyOrdered.size()==2,"dependency-order plugin fixture is complete");
    Check(dependencyOrdered[0].Id=="Provider"&&dependencyOrdered[1].Id=="Consumer",
        "plugin dependencies are the only supported reason to override preferred plugins.txt order");

    const auto migrationRoot=root/"migration";
    const auto legacy=migrationRoot/"RuneSchema.Networking";
    const auto renamed=migrationRoot/"RSNetworking";
    Write(legacy/"plugin.json",R"({"SchemaVersion":1,"Id":"RuneSchema.Networking","Version":"0.7.8"})");
    Write(renamed/"plugin.json",R"({"SchemaVersion":1,"Id":"RSNetworking","Version":"0.7.9"})");
    AddPakTriplet(legacy);AddPakTriplet(renamed);
    Write(migrationRoot/"plugins.txt","RuneSchema.Networking:0\nRSNetworking:1\n");
    const auto migrated=PS::PluginCatalog::Discover(migrationRoot);
    Check(migrated.size()==1&&migrated[0].Id=="RSNetworking","renamed networking plugin suppresses leftover legacy folder");
    Check(migrated[0].Enabled,"new RSNetworking order entry wins after legacy alias normalization");
    const auto sharedRoot=root/"shared-provider",network=sharedRoot/"RSNetworking",shared=sharedRoot/"RuneSchema.RegistryBridge";
    Write(network/"plugin.json",R"({"SchemaVersion":99,"Id":"RSNetworking","Version":"0.8.0.3"})");
    Write(shared/"plugin.json",R"({"Id":"RuneSchema.RegistryBridge","Version":"0.8.0.3"})");
    AddPakTriplet(network);AddPakTriplet(shared);
    Write(network/"paks"/"RegistryBridge"/"RegistryBridge_P.pak","obsolete partial copy");
    const auto preferred=PS::PluginCatalog::Discover(sharedRoot);
    const auto current=std::find_if(preferred.begin(),preferred.end(),[](const auto& plugin){return plugin.Id=="RSNetworking";});
    Check(current!=preferred.end()&&current->PreferSharedRegistry&&PS::PluginCatalog::PakDirectories(*current).size()==1,"valid shared provider supersedes an incomplete retired networking copy");
    Write(sharedRoot/"plugins.txt","RuneSchema.RegistryBridge:0\nRSNetworking:1\n");
    const auto fallback=PS::PluginCatalog::Discover(sharedRoot);
    const auto legacyFallback=std::find_if(fallback.begin(),fallback.end(),[](const auto& plugin){return plugin.Id=="RSNetworking";});
    Check(legacyFallback!=fallback.end()&&!legacyFallback->PreferSharedRegistry,"disabled shared provider retains legacy validation");

    std::cout << "Plugin catalog compatibility contract passed.\n";
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
