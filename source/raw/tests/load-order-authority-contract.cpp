#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Load-order contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}
static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("Load-order contract is missing: ") + text);
}
static void Forbid(const std::string& source, const char* text) {
    if (source.find(text) != std::string::npos)
        throw std::runtime_error(std::string("Load-order contract forbids: ") + text);
}

int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Mod order, main loader, plugin catalog, and plugin host inputs are required");
    const auto modOrder = Read(argv[1]);
    const auto mainLoader = Read(argv[2]);
    const auto pluginCatalog = Read(argv[3]);
    const auto pluginHost = Read(argv[4]);

    Require(modOrder, "Explicit runeschema.txt order is authoritative");
    Require(modOrder, "resolved.insert(resolved.end(), entries.begin(), entries.end())");
    Require(modOrder, "Unlisted ordinary/numeric mods are appended in discovery order");
    Require(modOrder, "A disabled row wins");
    Require(modOrder, "Zero wins across duplicate rows");
    Require(modOrder, "FoldName(entry.Name)");
    Require(modOrder, "disabled in RuneSchema/mods/runeschema.txt");
    Forbid(modOrder, "UE4SS Mods/mods.txt");
    Forbid(modOrder, "ModOrderPolicy::Apply(entries");
    Forbid(modOrder, "ModOrderPolicy::Apply(resolved");
    Forbid(modOrder, "ModOrderPolicy::Apply(fallback");
    Forbid(modOrder, "std::sort(fallback");

    Require(mainLoader, "m_orderedMods = ModLoadOrder::Resolve(modsPath, discovered)");
    Require(mainLoader, "for(const auto& name:ModLoadOrder::Resolve(modsRoot,discovered))pakRoots.push_back(modsRoot/name)");
    Require(mainLoader, "no mod PAK directories were added (fail-closed)");
    Require(mainLoader, "no plugin PAK directories were added (fail-closed)");
    Forbid(mainLoader, "pakRoots.push_back(modsRoot);");
    Forbid(mainLoader, "pakRoots.push_back(runeSchemaRoot/\"plugins\");");

    Require(pluginCatalog, "left->second.Position<right->second.Position");
    Require(pluginCatalog, "dependency.first==result[i].Id");
    Require(pluginCatalog, "dependency cycle detected; using preferred plugins.txt order");
    Require(pluginHost, "manifests=PluginCatalog::Discover(root");
    Require(pluginHost, "for(const auto& manifest:manifests)");

    std::cout << "Load-order authority contract passed.\n";
}
