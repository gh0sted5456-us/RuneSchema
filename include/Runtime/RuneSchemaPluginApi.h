#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define RS_PLUGIN_CALL __cdecl
#else
#define RS_PLUGIN_CALL
#endif

extern "C" {
inline constexpr uint32_t RUNESCHEMA_PLUGIN_API_VERSION = 1;
inline constexpr uint32_t RUNESCHEMA_PLUGIN_MAX_MESSAGE = 16 * 1024 * 1024;

enum RuneSchemaPluginResult : int32_t {
    RS_PLUGIN_OK = 0,
    RS_PLUGIN_INVALID_ARGUMENT = 1,
    RS_PLUGIN_INCOMPATIBLE_API = 2,
    RS_PLUGIN_DUPLICATE = 3,
    RS_PLUGIN_NOT_FOUND = 4,
    RS_PLUGIN_BUFFER_TOO_SMALL = 5,
    RS_PLUGIN_FAILED = 6
};
enum RuneSchemaPluginLogLevel : uint32_t { RS_LOG_INFO=0,RS_LOG_WARNING=1,RS_LOG_ERROR=2 };

using RuneSchemaServiceFn = int32_t (RS_PLUGIN_CALL *)(void* context,const char* requestJson,
    char* responseJson,uint32_t responseCapacity,uint32_t* responseSize);
struct RuneSchemaPluginDescriptor {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char* Id;
    const char* Name;
    const char* Version;
};
struct RuneSchemaHostApi {
    uint32_t StructSize;
    uint32_t ApiVersion;
    void (RS_PLUGIN_CALL *Log)(uint32_t level,const char* pluginId,const char* message);
    int32_t (RS_PLUGIN_CALL *RegisterCapability)(const char* pluginId,const char* capability);
    int32_t (RS_PLUGIN_CALL *RegisterService)(const char* pluginId,const char* service,
        RuneSchemaServiceFn callback,void* context);
    int32_t (RS_PLUGIN_CALL *CallService)(const char* callerId,const char* service,
        const char* requestJson,char* responseJson,uint32_t responseCapacity,uint32_t* responseSize);
};
using RuneSchemaPluginQueryFn = const RuneSchemaPluginDescriptor* (RS_PLUGIN_CALL *)();
using RuneSchemaPluginInitializeFn = int32_t (RS_PLUGIN_CALL *)(const RuneSchemaHostApi*,void**);
using RuneSchemaPluginShutdownFn = void (RS_PLUGIN_CALL *)(void*);
using RuneSchemaPluginPhaseFn = int32_t (RS_PLUGIN_CALL *)(void*);
}
