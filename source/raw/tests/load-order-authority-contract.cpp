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
    if (argc != 4) throw std::runtime_error("Mod order, main loader, and plugin catalog inputs are required");
    const auto modOrder = Read(argv[1]);
    const auto mainLoader = Read(argv[2]);
    const auto pluginCatalog = Read(argv[3]);

    Require(modOrder, "Explicit runeschema.txt order is authoritative");
    Require(modOrder, "resolved.insert(resolved.end(), entries.begin(), entries.end())");
    Forbid(modOrder, "ModOrderPolicy::Apply(entries");
    Forbid(modOrder, "ModOrderPolicy::Apply(resolved");

    Require(mainLoader, "m_orderedMods = ModLoadOrder::Resolve(modsPath, discovered)");
    Require(mainLoader, "for(const auto& name:ModLoadOrder::Resolve(modsRoot,discovered))pakRoots.push_back(modsRoot/name)");

    Require(pluginCatalog, "left->second.Position<right->second.Position");
    Require(pluginCatalog, "dependency.first==result[i].Id");
    Require(pluginCatalog, "dependency cycle detected; using manifest order");

    std::cout << "Load-order authority contract passed.\n";
}
