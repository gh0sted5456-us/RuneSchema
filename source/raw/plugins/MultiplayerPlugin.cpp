#include "Runtime/MultiplayerPolicy.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "Core/ConfigFiles.h"
#include <windows.h>

#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>

namespace {
using Json = nlohmann::json;
using namespace PS;
constexpr RuneSchemaPluginDescriptor Descriptor{
    sizeof(RuneSchemaPluginDescriptor), RUNESCHEMA_PLUGIN_API_VERSION, "RSNetworking", "RSNetworking", RS_MULTIPLAYER_VERSION
};
constexpr const char* Capabilities[]{
    "bridge.registry", "bridge.world", "bridge.identity", "bridge.plugin-connection",
    "bridge.plugin-state", "bridge.plugin-presentation", "presentation.dialogue-cue",
    "presentation.registry", "presentation.world-state", "multiplayer.policy"
};

struct Policy {
    std::mutex Gate;
    std::filesystem::path File;
    Json Rules = Json::object();
    uint64_t Revision = 1;
    std::string Error;
};

int32_t Reply(const Json& document, char* response, uint32_t capacity, uint32_t* size) {
    if (!size) return RS_PLUGIN_INVALID_ARGUMENT;
    const auto text = document.dump();
    if (text.size() >= RUNESCHEMA_PLUGIN_MAX_MESSAGE) return RS_PLUGIN_FAILED;
    *size = static_cast<uint32_t>(text.size() + 1);
    if (!response || capacity < *size) return RS_PLUGIN_BUFFER_TOO_SMALL;
    std::memcpy(response, text.c_str(), *size);
    return RS_PLUGIN_OK;
}

int32_t RS_PLUGIN_CALL PolicyService(void* context, const char* requestJson, char* response,
                                    uint32_t capacity, uint32_t* size) {
    try {
        auto& policy = *static_cast<Policy*>(context);
        const auto request = Json::parse(requestJson);
        std::lock_guard lock(policy.Gate);
        const auto action = request.at("Operation").get<std::string>();
        if (action == "Read")
            return Reply({{"Ok", policy.Error.empty()}, {"Status", policy.Error}, {"Rules", policy.Rules},
                          {"Revision", policy.Revision}}, response, capacity, size);
        if (action != "Set" && action != "Reset") return RS_PLUGIN_INVALID_ARGUMENT;
        if (!policy.Error.empty()) throw std::runtime_error(policy.Error);
        if (request.at("Revision").get<uint64_t>() != policy.Revision)
            throw std::runtime_error("Settings changed. Refresh before applying.");
        const auto key = request.at("Key").get<std::string>();
        auto next = policy.Rules;
        if (action == "Set") {
            (void)MultiplayerPolicy::Parse(request.at("Rule"));
            next[key] = request.at("Rule");
        } else next.erase(key);
        const auto result = Reply({{"Ok", true}, {"Revision", policy.Revision + 1}}, response, capacity, size);
        if (result != RS_PLUGIN_OK) return result; // A response-size probe cannot write.
        ConfigFiles::Write(policy.File, Json{{"Version", 1}, {"Rules", next}}.dump(2) + '\n');
        policy.Rules = std::move(next);
        ++policy.Revision;
        return result;
    } catch (const std::exception& error) {
        return Reply({{"Ok", false}, {"Status", error.what()}}, response, capacity, size);
    }
}
}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept {
    return &Descriptor;
}
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(
    const RuneSchemaHostApi* host, void** instance) noexcept {
    if (!host || !instance || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION
        || host->StructSize < offsetof(RuneSchemaHostApi, FindObject)
        || !host->CallService || !host->RegisterService || !host->RegisterCapability || !host->Log)
        return RS_PLUGIN_INCOMPATIBLE_API;
    *instance = nullptr;
    try {
        auto policy = std::make_unique<Policy>();
        uint32_t size = 0;
        constexpr auto request = R"({"Action":"MultiplayerPolicyLocation"})";
        if (host->CallService(Descriptor.Id, "runeschema.tools", request, nullptr, 0, &size) != RS_PLUGIN_BUFFER_TOO_SMALL
            || !size || size > RUNESCHEMA_PLUGIN_MAX_MESSAGE) return RS_PLUGIN_INCOMPATIBLE_API;
        std::string result(size, '\0');
        if (host->CallService(Descriptor.Id, "runeschema.tools", request, result.data(), size, &size) != RS_PLUGIN_OK)
            return RS_PLUGIN_INCOMPATIBLE_API;
        policy->File = std::filesystem::u8path(Json::parse(result.c_str()).at("Path").get<std::string>());
        try {
            policy->File = std::filesystem::weakly_canonical(policy->File.parent_path()) / policy->File.filename();
            const auto attributes = GetFileAttributesW(policy->File.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("The multiplayer settings file redirects to another path.");
            if (std::filesystem::exists(policy->File)) {
                const auto settings = Json::parse(ConfigFiles::Read(policy->File));
                if (settings.value("Version", 0) != 1 || !settings.at("Rules").is_object())
                    throw std::runtime_error("Unrecognized multiplayer settings.");
                for (const auto& rule : settings["Rules"]) (void)MultiplayerPolicy::Parse(rule);
                policy->Rules = settings["Rules"];
            }
        } catch (const std::exception& error) {
            policy->Error = std::string("Multiplayer settings were preserved: ") + error.what();
            host->Log(RS_LOG_WARNING, Descriptor.Id, policy->Error.c_str());
        }
        for (const auto* capability : Capabilities)
            if (host->RegisterCapability(Descriptor.Id, capability) != RS_PLUGIN_OK) return RS_PLUGIN_DUPLICATE;
        if (host->RegisterService(Descriptor.Id, MultiplayerPolicy::Service, PolicyService, policy.get()) != RS_PLUGIN_OK)
            return RS_PLUGIN_DUPLICATE;
        *instance = policy.release();
        return RS_PLUGIN_OK;
    } catch (...) { return RS_PLUGIN_FAILED; }
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* instance) noexcept {
    delete static_cast<Policy*>(instance);
}
