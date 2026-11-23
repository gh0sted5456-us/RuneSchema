// RuneSchema.Wiki, in-game half: the "RuneSchema.Wiki.lookup" service behind "rs wiki <text>".
// It sends an owned plugin event to the Helpy half and asks Helpy to open the Wiki tab.
// Nothing is sent unless the reply buffer is already large enough, so a sizing call never acts.
#include "Core/HelpyBranding.h"
#include "Runtime/RuneSchemaPluginApi.h"
#include "WikiLinks.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cstddef>
#include <cstring>

namespace {
using Json = nlohmann::json;
constexpr RuneSchemaPluginDescriptor Descriptor{sizeof(RuneSchemaPluginDescriptor), RUNESCHEMA_PLUGIN_API_VERSION, "RuneSchema.Wiki", "Dragonwilds Wiki", PS::HelpyBranding::Version};
constexpr uint32_t ReplyCapacity = 256; // every reply below fits
const RuneSchemaHostApi* Host = nullptr;

int32_t Reply(bool opened, const char* status, char* output, uint32_t capacity, uint32_t* size) {
    const auto text = Json{{"Opened", opened}, {"Status", status}}.dump();
    if (text.size() + 1 > capacity) return RS_PLUGIN_FAILED;
    std::memcpy(output, text.c_str(), text.size() + 1); *size = static_cast<uint32_t>(text.size() + 1);
    return RS_PLUGIN_OK;
}

int32_t RS_PLUGIN_CALL Lookup(void*, const char* input, char* output, uint32_t capacity, uint32_t* size) {
    if (!input || !size || (!output && capacity) || strnlen_s(input, 4097) > 4096) return RS_PLUGIN_INVALID_ARGUMENT;
    if (!output || capacity < ReplyCapacity) { *size = ReplyCapacity; return RS_PLUGIN_BUFFER_TOO_SMALL; }
    *size = 0;
    if (!Host) return RS_PLUGIN_NOT_FOUND;
    std::string query;
    try {
        const auto request = Json::parse(input);
        query = request.value("Query", std::string{});
        if (query.size() > PS::WikiLinks::MaxQueryBytes) return Reply(false, "That wiki search is too long.", output, capacity, size);
    } catch (...) { return RS_PLUGIN_INVALID_ARGUMENT; }
    const auto event = Json{{"Event", "Lookup"}, {"Payload", {{"Query", query}}}, {"Open", "RuneSchema.Wiki.wiki"}}.dump();
    char ignored[64]{}; uint32_t ignoredSize = 0;
    switch (Host->CallService(Descriptor.Id, "helpy.plugin.event", event.c_str(), ignored, sizeof(ignored), &ignoredSize)) {
    case RS_PLUGIN_OK: return Reply(true, "Opening the wiki in Helpy.", output, capacity, size);
    case RS_PLUGIN_NOT_FOUND: return Reply(false, "Open Helpy first: the wiki shows in Helpy's window or overlay.", output, capacity, size);
    case RS_PLUGIN_FAILED: return Reply(false, "Too many wiki requests at once. Try again in a moment.", output, capacity, size);
    default: return Reply(false, "That wiki search could not be sent to Helpy.", output, capacity, size);
    }
}
}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept { return &Descriptor; }
extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(const RuneSchemaHostApi* host, void** instance) noexcept {
    if (!host || !instance || host->StructSize < offsetof(RuneSchemaHostApi, FindObject) || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION
        || !host->RegisterService || !host->CallService) return RS_PLUGIN_INCOMPATIBLE_API;
    *instance = nullptr;
    Host = host;
    if (host->RegisterService(Descriptor.Id, "RuneSchema.Wiki.lookup", Lookup, nullptr) != RS_PLUGIN_OK) { Host = nullptr; return RS_PLUGIN_DUPLICATE; }
    *instance = const_cast<RuneSchemaPluginDescriptor*>(&Descriptor);
    return RS_PLUGIN_OK;
}
extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept { Host = nullptr; }
