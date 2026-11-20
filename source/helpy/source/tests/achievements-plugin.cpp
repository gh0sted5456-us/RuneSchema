#include "Runtime/RuneSchemaPluginApi.h"
#include "Core/AchievementSources.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cassert>
#include <cstring>
#include <map>
#include <fstream>
#include <iostream>
#include <string>
namespace {
using Json = nlohmann::json;
RuneSchemaServiceFn Service = nullptr; void* Context = nullptr; bool WrongHost = false;
void RS_PLUGIN_CALL Log(uint32_t, const char*, const char*) {}
int32_t RS_PLUGIN_CALL Capability(const char* owner, const char* id) { return std::string_view(owner) == "RuneSchema.Achievements" && std::string_view(id) == "helpy.achievements" ? RS_PLUGIN_OK : RS_PLUGIN_INVALID_ARGUMENT; }
int32_t RS_PLUGIN_CALL Register(const char* owner, const char* id, RuneSchemaServiceFn call, void* context) { if (Capability(owner, id)) return RS_PLUGIN_INVALID_ARGUMENT; Service = call; Context = context; return RS_PLUGIN_OK; }
int32_t RS_PLUGIN_CALL Call(const char*, const char* name, const char*, char* output, uint32_t capacity, uint32_t* size) {
    assert(std::string_view(name) == "helpy.host");
    const auto text = Json{{"Process", WrongHost ? "Game" : "Helpy"}}.dump(); *size = uint32_t(text.size() + 1);
    if (capacity < *size) return RS_PLUGIN_BUFFER_TOO_SMALL; memcpy(output, text.c_str(), *size); return RS_PLUGIN_OK;
}
Json Exchange(const Json& request) {
    const auto text = request.dump(); std::string output(1 << 16, '\0'); uint32_t size = 0;
    assert(Service(Context, text.c_str(), output.data(), uint32_t(output.size()), &size) == RS_PLUGIN_OK);
    return Json::parse(output.c_str());
}
void Write(const std::filesystem::path& file, const std::string& text) { std::filesystem::create_directories(file.parent_path()); std::ofstream(file, std::ios::binary) << text; }
std::map<std::string, Json> ByOwner(const Json& reply) { std::map<std::string, Json> result; for (const auto& source : reply.at("Sources")) result[source.at("Owner").get<std::string>()] = source; return result; }
}
int main(int argc, char** argv) {
    assert(argc == 2); namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("RS-achievements-plugin-" + std::to_string(GetCurrentProcessId())); assert(!fs::exists(root));
    struct Cleanup { fs::path Root; ~Cleanup() { std::error_code error; fs::remove_all(Root, error); } } cleanup{root};
    fs::create_directories(root / "dll"); const auto dll = root / "dll/RuneSchema.Achievements.dll"; fs::copy_file(fs::u8path(argv[1]), dll);
    const auto module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32); assert(module);
    const auto start = reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module, "RuneSchemaPlugin_Initialize"));
    const auto query = reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module, "RuneSchemaPlugin_Query"));
    assert(start && query && std::string_view(query()->Id) == "RuneSchema.Achievements");
    RuneSchemaHostApi host{sizeof(RuneSchemaHostApi), RUNESCHEMA_PLUGIN_API_VERSION, Log, Capability, Register, Call}; void* instance = nullptr;
    WrongHost = true; assert(start(&host, &instance) == RS_PLUGIN_INCOMPATIBLE_API && !Service); WrongHost = false;
    assert(start(&host, &instance) == RS_PLUGIN_OK && instance && Service);

    // UE4SS Mods folder with a RuneSchema install inside it.
    const auto mods = root / "Mods";
    Write(mods / "mods.txt", "; comment\nListedOn : 1\nListedOff : 0\nRuneSchema : 1\nCacheOnly : 1\n");
    Write(mods / "ListedOn/achievements.json", "{}"); Write(mods / "ListedOn/cache/achievements.json", "{}");
    Write(mods / "CacheOnly/cache/achievements.json", "{}"); // A UE4SS mod shipping cache/achievements.json, as JBM does.
    Write(mods / "ListedOff/achievements.json", "{}");
    Write(mods / "Toggle/enabled.txt", ""); Write(mods / "Toggle/achievements.json", "{}");
    Write(mods / "NoFile/Scripts/main.lua", "");
    Write(mods / "shared/achievements.json", "{}");
    Write(mods / "RuneSchema/achievements.json", "{}");
    const auto rs = mods / "RuneSchema/mods";
    Write(rs / "RootFile/achievements.json", "{}");
    Write(rs / "CacheFile/cache/achievements.json", "{}");
    Write(rs / "Off/achievements.json", "{}");
    const auto text = [](const fs::path& path) { return PS::AchievementSources::Text(path); };
    const Json rows = Json::array({
        {{"Folder", text(rs / "RootFile")}, {"Name", "Root File"}, {"Enabled", true}, {"Present", true}},
        {{"Folder", text(rs / "CacheFile")}, {"Name", "Cache File"}, {"Enabled", true}, {"Present", true}},
        {{"Folder", text(rs / "Off")}, {"Name", "Off"}, {"Enabled", false}, {"Present", true}},
        {{"Folder", text(rs / "Missing")}, {"Name", "Missing"}, {"Enabled", true}, {"Present", false}}});

    auto found = ByOwner(Exchange({{"Op", "discover"}, {"Mods", text(mods)}, {"RuneSchema", rows}}));
    assert(found.size() == 5);
    assert(found.at("ue4ss/listedon").at("Kind") == "ue4ss" && found.at("ue4ss/toggle").at("Enabled") == true);
    assert(found.at("ue4ss/listedon").at("File") == text(mods / "ListedOn/achievements.json"));          // The root file wins.
    assert(found.at("ue4ss/cacheonly").at("File") == text(mods / "CacheOnly/cache/achievements.json"));
    assert(found.at("mods/rootfile").at("File") == text(rs / "RootFile/achievements.json"));
    assert(found.at("mods/cachefile").at("File") == text(rs / "CacheFile/cache/achievements.json") && found.at("mods/cachefile").at("Name") == "Cache File");
    assert(!found.contains("ue4ss/runeschema") && !found.contains("ue4ss/shared") && !found.contains("ue4ss/listedoff") && !found.contains("mods/off"));

    found = ByOwner(Exchange({{"Op", "discover"}, {"Mods", text(mods)}, {"RuneSchema", rows}, {"IncludeDisabled", true}}));
    assert(found.size() == 7 && found.at("ue4ss/listedoff").at("Enabled") == false && found.at("mods/off").at("Enabled") == false);

    found = ByOwner(Exchange({{"Op", "discover"}, {"Mods", text(mods)}, {"RuneSchema", rows}, {"UE4SS", false}}));
    assert(found.size() == 2 && found.contains("mods/rootfile"));

    // A junction never pulls achievements from outside the Mods folder.
    const auto outside = root / "outside"; Write(outside / "achievements.json", "{}");
    std::error_code error; fs::create_directory_symlink(outside, mods / "Linked", error);
    if (!error) { Write(mods / "mods.txt", "Linked : 1\n"); assert(!ByOwner(Exchange({{"Op", "discover"}, {"Mods", text(mods)}})).contains("ue4ss/linked")); fs::remove(mods / "Linked"); }

    uint32_t size = 0;
    assert(Service(Context, R"({"Op":"discover","Mods":"relative\\path"})", nullptr, 0, &size) == RS_PLUGIN_INVALID_ARGUMENT);
    assert(Service(Context, R"({"Op":"delete"})", nullptr, 0, &size) == RS_PLUGIN_INVALID_ARGUMENT);
    assert(Exchange({{"Op", "read"}}).at("Loaded") == true);
    std::cout << "Achievements plugin: Helpy-only host, UE4SS mods.txt and enabled.txt, root and cache files for UE4SS and RuneSchema mods, disabled filter, RuneSchema/shared skipped and links refused passed.\n";
}
