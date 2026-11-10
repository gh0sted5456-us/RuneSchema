#include "Runtime/RuneSchemaPluginApi.h"

namespace {
constexpr RuneSchemaPluginDescriptor Plugin{
    sizeof(RuneSchemaPluginDescriptor), RUNESCHEMA_PLUGIN_API_VERSION,
    "RuneSchema.UE4SS.Bridge", "RuneSchema UE4SS Bridge", "0.8.1.0"
};
}

extern "C" __declspec(dllexport) const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query()
{
    return &Plugin;
}

extern "C" __declspec(dllexport) int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(
    const RuneSchemaHostApi* host, void** instance)
{
    if (!host || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION
        || host->StructSize < offsetof(RuneSchemaHostApi, RegisterService)
        || !host->RegisterCapability) return RS_PLUGIN_INCOMPATIBLE_API;
    if (!instance) return RS_PLUGIN_INVALID_ARGUMENT;
    *instance = nullptr;
    return host->RegisterCapability(Plugin.Id, "ue4ss.sister-content");
}

extern "C" __declspec(dllexport) void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) {}
