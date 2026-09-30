#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Order JSONC contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}
static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("Order JSONC contract is missing: ") + text);
}
int main(int argc, char** argv) {
    if (argc != 5) throw std::runtime_error("Four order/config inputs are required");
    const auto modOrder = Read(argv[1]);
    const auto pluginCatalog = Read(argv[2]);
    const auto ui = Read(argv[3]);
    const auto packagedPlugins = Read(argv[4]);

    Require(modOrder, "runeschema.jsonc");
    Require(modOrder, "runeschema.txt");
    Require(modOrder, "Migrated runeschema.txt to runeschema.jsonc");
    Require(modOrder, "nlohmann::json::parse(file, nullptr, true, true)");
    Require(modOrder, "RemoveLegacyOrder(legacy)");

    Require(pluginCatalog, "plugins.jsonc");
    Require(pluginCatalog, "plugins.txt");
    Require(pluginCatalog, "WriteJsoncOrder(current,entries)");
    Require(pluginCatalog, "nlohmann::json::parse(stream,nullptr,true,true)");
    Require(pluginCatalog, "fs::remove(legacy,error)");

    Require(ui, "runeschema.jsonc load order");
    Require(packagedPlugins, "\"Plugins\"");
    Require(packagedPlugins, "\"Enabled\": true");

    std::cout << "Order JSONC migration contract passed.\n";
}
