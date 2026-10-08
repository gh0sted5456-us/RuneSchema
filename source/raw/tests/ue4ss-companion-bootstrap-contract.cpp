#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("UE4SS companion bootstrap contract input is unavailable");
    return {std::istreambuf_iterator<char>(file), {}};
}
static void Require(const std::string& source, const char* text) {
    if (source.find(text) == std::string::npos)
        throw std::runtime_error(std::string("UE4SS companion bootstrap contract is missing: ") + text);
}
static void Forbid(const std::string& source, const char* text) {
    if (source.find(text) != std::string::npos)
        throw std::runtime_error(std::string("UE4SS companion bootstrap contains prohibited capability: ") + text);
}

int main(int argc, char** argv) {
    if (argc != 4) throw std::runtime_error("bootstrap Lua, build.ps1, and authoring guide are required");
    const auto lua = Read(argv[1]);
    const auto build = Read(argv[2]);
    const auto guide = Read(argv[3]);

    Require(lua, "mods_root .. \"/runeschema.txt\"");
    Require(lua, "entry.name .. \"/ue4ss\"");
    Require(lua, "scripts_root .. \"/main.lua\"");
    Require(lua, "entries[key].enabled = entries[key].enabled and value == \"1\"");
    Require(lua, "xpcall(function()");
    Require(lua, "dofile(main_path)");
    Require(lua, "RuneSchemaUE4SS.Current");
    Require(lua, "package.path = package.path");
    Forbid(lua, "package.cpath");
    Forbid(lua, ".dll");

    Require(build, "ue4ss\\scripts\\main.lua");
    Require(build, "scripts\\main.lua");
    Require(guide, "ue4ss/scripts/main.lua");
    Require(guide, "Lua only");
    Require(guide, "mods.txt");

    std::cout << "UE4SS companion bootstrap contract passed.\n";
}
