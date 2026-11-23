// Helpy Achievements plugin. Finds achievements.json in UE4SS mods and RuneSchema mods so any mod can award
// achievements without depending on RuneSchema's mod format. Helpy turns achievements off when this plugin is
// missing or disabled in RuneSchema/settings/plugins.txt ("RuneSchema.Achievements : 0").
#include "Core/AchievementSources.h"
#include "Core/HelpyBranding.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cstring>
namespace {
using Json = nlohmann::json;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor), RUNESCHEMA_PLUGIN_API_VERSION, "RuneSchema.Achievements", "Helpy Achievements", PS::HelpyBranding::Version};
int32_t Reply(const Json& value, char* output, uint32_t capacity, uint32_t* size) {
    const auto text = value.dump();
    if (text.size() + 1 > RUNESCHEMA_PLUGIN_MAX_MESSAGE) return RS_PLUGIN_FAILED;
    *size = uint32_t(text.size() + 1);
    if (!output || capacity < *size) return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(output, text.c_str(), *size);
    return RS_PLUGIN_OK;
}
int32_t RS_PLUGIN_CALL Exchange(void*, const char* input, char* output, uint32_t capacity, uint32_t* size) {
    if (!input || !size || (!output && capacity) || strnlen_s(input, 1024 * 1024 + 1) > 1024 * 1024) return RS_PLUGIN_INVALID_ARGUMENT;
    *size = 0;
    try {
        const auto request = Json::parse(input, [](int depth, Json::parse_event_t, Json&) { if (depth > 16) throw std::runtime_error("Request nesting limit"); return true; });
        const auto op = request.at("Op").get<std::string>();
        if (op == "read") return Reply({{"Loaded", true}, {"Name", Descriptor.Name}, {"Version", Descriptor.Version}, {"Sources", Json::array({"ue4ss", "runeschema"})}}, output, capacity, size);
        if (op != "discover") return RS_PLUGIN_INVALID_ARGUMENT;
        const auto mods = request.contains("Mods") && request["Mods"].is_string() ? std::filesystem::u8path(request["Mods"].get<std::string>()) : std::filesystem::path{};
        if (!mods.empty() && !mods.is_absolute()) return RS_PLUGIN_INVALID_ARGUMENT;
        return Reply(PS::AchievementSources::Discover(mods, request.value("RuneSchema", Json::array()), request.value("UE4SS", true), request.value("IncludeDisabled", false)), output, capacity, size);
    } catch (...) { return RS_PLUGIN_INVALID_ARGUMENT; }
}
}
extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept { return &Descriptor; }
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host, void** instance) noexcept {
    if (!host || !instance || host->StructSize < offsetof(RuneSchemaHostApi, FindObject) || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION || !host->RegisterService || !host->RegisterCapability || !host->CallService) return RS_PLUGIN_INCOMPATIBLE_API;
    *instance = nullptr;
    try {
        char identity[256]{}; uint32_t size = 0;
        if (host->CallService(Descriptor.Id, "helpy.host", "{}", identity, sizeof(identity), &size) != RS_PLUGIN_OK || Json::parse(identity).value("Process", std::string{}) != "Helpy") return RS_PLUGIN_INCOMPATIBLE_API;
        if (host->RegisterCapability(Descriptor.Id, "helpy.achievements") != RS_PLUGIN_OK || host->RegisterService(Descriptor.Id, "helpy.achievements", Exchange, nullptr) != RS_PLUGIN_OK) return RS_PLUGIN_DUPLICATE;
        *instance = const_cast<RuneSchemaPluginDescriptor*>(&Descriptor);
        return RS_PLUGIN_OK;
    } catch (...) { return RS_PLUGIN_FAILED; }
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept {}
