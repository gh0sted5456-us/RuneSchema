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

int main()
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

    bool missingDependency = false;
    bool cycle = false;
    for (const auto& diagnostic : diagnostics) {
        missingDependency = missingDependency || diagnostic.find("dependency NotInstalled is unavailable") != std::string::npos;
        cycle = cycle || diagnostic.find("dependency cycle detected") != std::string::npos;
    }
    Check(missingDependency, "missing dependency is diagnosed without suppressing discovery");
    Check(cycle, "dependency cycle falls back to deterministic best-effort order");

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

    std::cout << "Plugin catalog compatibility contract passed.\n";
}
