#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define RS_PLUGIN_CALL __cdecl
#else
#define RS_PLUGIN_CALL
#endif

extern "C" {
// ABI version. Semantic RuneSchema and plugin versions are independent.
inline constexpr uint32_t RUNESCHEMA_PLUGIN_API_VERSION = 1;
// Maximum request or response buffer accepted by the service router.
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
// Return RS_PLUGIN_OK to continue enumeration. Any other result stops it.
using RuneSchemaObjectVisitorFn = int32_t (RS_PLUGIN_CALL *)(void* context,void* object);
// Returned by RuneSchemaPlugin_Query. Strings must remain valid until unload.
struct RuneSchemaPluginDescriptor {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char* Id;
    const char* Name;
    const char* Version;
};
struct RuneSchemaModApiV1;
// UI callbacks run on Helpy's UI thread. Strings and callbacks live until plugin unload.
// Use host widgets so plugins do not depend on Helpy's private ImGui ABI.
struct RuneSchemaCompanionV1 {
    uint32_t StructSize;
    const wchar_t* Executable;
    const wchar_t* WindowClass;
    const wchar_t* WindowTitle;
};
struct RuneSchemaNavUiV1 {
    uint32_t StructSize;
    void* Context;
    void (RS_PLUGIN_CALL *Text)(void*,const char*);
    int32_t (RS_PLUGIN_CALL *Button)(void*,const char*);
    void (RS_PLUGIN_CALL *Separator)(void*);
    // Optional append-only UI extension: one companion surface in this workspace.
    int32_t (RS_PLUGIN_CALL *Companion)(void*,const RuneSchemaCompanionV1*);
    void (RS_PLUGIN_CALL *ReturnHome)(void*);
};
using RuneSchemaNavDrawFn = int32_t (RS_PLUGIN_CALL *)(void*,const RuneSchemaNavUiV1*);
struct RuneSchemaNavTabV1 {
    uint32_t StructSize;
    const char* Route; // stable, namespaced as <pluginId>.<route>
    const char* Title;
    const char* Icon; // short text/glyph for the collapsed sidebar
    RuneSchemaNavDrawFn Draw;
    void* Context;
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
    void* (RS_PLUGIN_CALL *FindObject)(void* objectClass,void* objectPackage,const wchar_t* name,int32_t exactClass);
    int32_t (RS_PLUGIN_CALL *ForEachObjectOfClass)(void* objectClass,int32_t includeDerived,
        uint64_t excludeFlags,uint32_t exclusionInternalFlags,RuneSchemaObjectVisitorFn visitor,void* context);
    void* (RS_PLUGIN_CALL *FindFieldClass)(const wchar_t* name);
    int32_t (RS_PLUGIN_CALL *IsFieldA)(void* field,void* fieldClass);
    // Append-only API 1 extension. Check StructSize before reading this field.
    // Returns an allow-listed, validated core binding or nullptr.
    void* (RS_PLUGIN_CALL *ResolveBinding)(const char* callerId,const char* binding);
    // Append-only API 1 extension: share one serial allocator across native plugins.
    int32_t (RS_PLUGIN_CALL *ObjectIdentity)(void* object,int32_t* index,int32_t* serial);
    void* (RS_PLUGIN_CALL *ResolveObjectIdentity)(int32_t index,int32_t serial);
    // Optional append-only API 1 extension; check StructSize and the pointer.
    const RuneSchemaModApiV1* (RS_PLUGIN_CALL *GetModApi)(const char* pluginId);
    // Optional append-only API 1 extension. Available only in Host=helpy.
    int32_t (RS_PLUGIN_CALL *RegisterNavTab)(const char* pluginId,const RuneSchemaNavTabV1* tab);
};
// Required exports: Query, Initialize, and Shutdown.
using RuneSchemaPluginQueryFn = const RuneSchemaPluginDescriptor* (RS_PLUGIN_CALL *)();
using RuneSchemaPluginInitializeFn = int32_t (RS_PLUGIN_CALL *)(const RuneSchemaHostApi*,void**);
using RuneSchemaPluginShutdownFn = void (RS_PLUGIN_CALL *)(void*);
// Optional exports: OnUiInit and OnUnrealInit.
using RuneSchemaPluginPhaseFn = int32_t (RS_PLUGIN_CALL *)(void*);
}
