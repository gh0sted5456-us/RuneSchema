#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static std::string Read(const std::filesystem::path& path) {
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
    Require(guide, "complete Lua UE4SS packages");
    Require(guide, "Nested native or hybrid DLL packages");
    Require(guide, "mods.txt");
    Require(lua,"RUNESCHEMA_UE4SS_PACKAGE_SESSION");
    Require(lua,"packages-loader.lua");
    Require(build,"scripts\\packages-loader.lua");
    Require(build,"scripts\\package-discovery.lua");
    Require(build,"ManagedUE4SSHost");
    const auto helper=Read(std::filesystem::path(argv[1]).parent_path()/"packages-loader.lua");
    Require(helper,"Stale package manifest refused");
    Require(helper,"loadfile");
    Require(helper,"entry.Kind ~= \"lua\"");
    Require(helper,"only Lua add-ons are supported");
    Forbid(helper,"matching UE4SS host required");
    Forbid(helper,"package.cpath");
    const auto discovery=Read(std::filesystem::path(argv[1]).parent_path()/"package-discovery.lua");
    Require(discovery,"# Session\\t");
    Require(discovery,"entry.Name] = node");
    Require(discovery,"table.pack(original(...))");
    Forbid(discovery,"LoopAsync");
    Forbid(discovery,"io.popen");

    const auto sourceRoot=std::filesystem::path(argv[1]).parent_path().parent_path().parent_path();
    const auto integration=Read(sourceRoot/"raw/include/Runtime/UE4SSPackageIntegration.h");
    Require(integration,"[NATIVE-UNSUPPORTED]");
    Forbid(integration,"QueuePackage");
    Forbid(integration,"GetProcAddress");
    Forbid(integration,"LoadLibrary");
    Require(build,"Custom UE4SS hosts are retired");
    Forbid(build,"$targets += 'UE4SS'");
    Forbid(build,"-HostDll $universal.Host");

    std::cout << "UE4SS companion bootstrap contract passed.\n";
}
