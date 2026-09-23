#include "Runtime/PluginCatalog.h"

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
    Write(root / "plugins.txt", "Alpha:1\nBeta:1\n");

    std::vector<std::string> diagnostics;
    const auto plugins = PS::PluginCatalog::Discover(root, &diagnostics);
    Check(plugins.size() == 2, "mismatched and cyclic plugins remain discoverable");
    Check(!plugins[0].EntryPoint.empty() || !plugins[1].EntryPoint.empty(),
        "missing optional native entry point remains represented");
    for (const auto& plugin : plugins) {
        Check(PS::PluginCatalog::PakDirectories(plugin).size() == 1,
            "complete PAK triplet remains independently mountable");
    }

    bool missingDependency = false;
    bool cycle = false;
    for (const auto& diagnostic : diagnostics) {
        missingDependency = missingDependency || diagnostic.find("dependency NotInstalled is unavailable") != std::string::npos;
        cycle = cycle || diagnostic.find("dependency cycle detected") != std::string::npos;
    }
    Check(missingDependency, "missing dependency is diagnosed without suppressing discovery");
    Check(cycle, "dependency cycle falls back to deterministic best-effort order");

    std::cout << "Plugin catalog compatibility contract passed.\n";
}
