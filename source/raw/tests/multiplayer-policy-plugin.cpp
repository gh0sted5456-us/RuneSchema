#include "Runtime/MultiplayerPolicy.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <windows.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
fs::path Settings;
RuneSchemaServiceFn PolicyCallback = nullptr;
void* Context = nullptr;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void RS_PLUGIN_CALL Log(uint32_t, const char*, const char*) {}
int32_t RS_PLUGIN_CALL Capability(const char*, const char*) { return RS_PLUGIN_OK; }
int32_t RS_PLUGIN_CALL Register(const char* id, const char* name, RuneSchemaServiceFn callback, void* context) {
    Check(std::string_view(id) == "RSNetworking" && std::string_view(name) == PS::MultiplayerPolicy::Service,
          "unexpected plugin registration");
    PolicyCallback = callback;
    Context = context;
    return RS_PLUGIN_OK;
}
int32_t RS_PLUGIN_CALL Call(const char*, const char* service, const char* request, char* out, uint32_t capacity, uint32_t* size) {
    Check(std::string_view(service) == "runeschema.tools" && Json::parse(request).at("Action") == "MultiplayerPolicyLocation",
          "unexpected host call");
    const auto path = Settings.u8string();
    const auto reply = Json{{"Path", std::string(path.begin(), path.end())}}.dump();
    *size = static_cast<uint32_t>(reply.size() + 1);
    if (!out || capacity < *size) return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(out, reply.c_str(), *size);
    return RS_PLUGIN_OK;
}
Json Request(const Json& request) {
    const auto input = request.dump();
    char buffer[4096]{};
    uint32_t size = 0;
    Check(PolicyCallback(Context, input.c_str(), buffer, sizeof(buffer), &size) == RS_PLUGIN_OK, "policy service reply");
    return Json::parse(buffer);
}
}

int wmain(int argc, wchar_t** argv) try {
    using namespace PS::MultiplayerPolicy;
    Check(argc == 2, "pass the native policy DLL path");
    Check(ActorClass("/Example/Actors/BP_Test.BP_Test_C") && ActorClass("/Script/Example.ReplicatedActor"),
          "cooked and native actor declarations");
    Check(!ActorClass("/Example/../BP_Test.BP_Test_C") && !ActorClass("/Example/BP_Test.BP_Test"), "invalid class path");
    const auto parsed = Parse({{"RangeMeters", 120}, {"UpdatesPerSecond", 12.5}});
    Check(parsed.DistanceSquared == 144000000.f && parsed.Frequency == 12.5f, "engine units");
    auto module = LoadLibraryExW(argv[1], nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    Check(module != nullptr, "load compiled plugin");
    const auto query = reinterpret_cast<RuneSchemaPluginQueryFn>(GetProcAddress(module, "RuneSchemaPlugin_Query"));
    const auto start = reinterpret_cast<RuneSchemaPluginInitializeFn>(GetProcAddress(module, "RuneSchemaPlugin_Initialize"));
    const auto stop = reinterpret_cast<RuneSchemaPluginShutdownFn>(GetProcAddress(module, "RuneSchemaPlugin_Shutdown"));
    Check(query && start && stop && std::string_view(query()->Id) == "RSNetworking", "plugin exports and identity");
    const auto root = fs::temp_directory_path() / ("RuneSchemaMultiplayer-" + std::to_string(GetCurrentProcessId()));
    Check(fs::create_directory(root), "create isolated settings directory");
    struct Cleanup { fs::path Root; ~Cleanup() { std::error_code error; fs::remove_all(Root, error); } } cleanup{root};
    Settings = root / "multiplayer.json";
    const RuneSchemaHostApi host{sizeof(RuneSchemaHostApi), RUNESCHEMA_PLUGIN_API_VERSION, Log, Capability, Register, Call};
    void* instance = nullptr;
    Check(start(&host, &instance) == RS_PLUGIN_OK && instance, "initialize policy DLL");
    const Json set{{"Operation", "Set"}, {"Revision", 1}, {"Key", "Example:actor"}, {"Rule", {{"RangeMeters", 120}}}};
    uint32_t size = 0;
    Check(PolicyCallback(Context, set.dump().c_str(), nullptr, 0, &size) == RS_PLUGIN_BUFFER_TOO_SMALL && !fs::exists(Settings),
          "size probe must not save policy");
    auto invalid = set;
    invalid["Rule"]["RangeMeters"] = -1;
    Check(!Request(invalid).at("Ok").get<bool>() && !fs::exists(Settings), "invalid settings must not write");
    Check(Request(set).at("Ok") == true && fs::is_regular_file(Settings), "persist valid policy");
    Check(Request(set).at("Ok") == false, "stale revision cannot overwrite saved policy");
    stop(instance);
    Check(start(&host, &instance) == RS_PLUGIN_OK && instance, "reload policy DLL");
    const auto reloaded = Request({{"Operation", "Read"}});
    Check(reloaded.at("Rules").at("Example:actor").at("RangeMeters") == 120, "saved settings survive restart");
    Check(Request({{"Operation", "Reset"}, {"Revision", reloaded.at("Revision")}, {"Key", "Example:actor"}}).at("Ok") == true,
          "reset overrides");
    Check(Request({{"Operation", "Read"}}).at("Rules").empty(), "default policy restored");
    stop(instance);
    FreeLibrary(module);
    std::cout << "Native multiplayer policy ABI, settings validation, persistence and reset passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
