#pragma once
#include <stdint.h>

#if defined(_WIN32)
#define RS_MOD_CALL __cdecl
#else
#define RS_MOD_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

enum { RS_MOD_API_VERSION=1, RS_MOD_MAX_INPUTS=256, RS_MOD_MAX_RESULTS=1024, RS_MOD_ERROR_BYTES=256 };
enum RuneSchemaModResult {
    RS_MOD_OK=0, RS_MOD_INVALID_ARGUMENT=1, RS_MOD_UNAVAILABLE=2,
    RS_MOD_NOT_FOUND=3, RS_MOD_CONFLICT=4, RS_MOD_BUSY=5, RS_MOD_FAILED=6
};

// Optional core export for Helpy mod achievements, separate from RuneSchemaModApiV1: find it with
// GetProcAddress(<RuneSchema dlls/main.dll>, "RuneSchema_ReportModEventV1") so the dependency stays soft.
// Pushes one local-player event of kind "report". owner: your achievement owner, "mods/<folder>" for a
// RuneSchema mod or "ue4ss/<folder>" for a UE4SS mod (case-insensitive). name: 1-64 of A-Z a-z 0-9 - _ .
// amount > 0 and finite (summed by "count"); value finite (compared with "atLeast"). parametersJson: null,
// or a JSON object of at most 4096 bytes, matched by "where" (held to 1 KiB like captured parameters).
// Any thread. Returns RS_MOD_OK, RS_MOD_INVALID_ARGUMENT or RS_MOD_FAILED.
typedef int32_t (*RuneSchemaReportModEventV1Fn)(const char* owner,const char* name,double amount,double value,const char* parametersJson);

// Optional RuneSchema_AuthorizeSettingsMenuPlayerV1 export. Game thread, authoritative host only.
// Trusted Settings Menu passes its native owning controller after verifying the signed PIN request.
// Saves full Helpy authorization by native character GUID; no PIN or client-selected identity is accepted.
// 1 = saved/already authorized, 0 = linking disabled, -1 = refused or not saved. Never throws.
typedef int32_t (RS_MOD_CALL *RuneSchemaAuthorizeSettingsMenuPlayerV1Fn)(void* playerController);

// Evaluate is serialized on a worker. It must not call Unreal or retain the buffers.
typedef int32_t (RS_MOD_CALL *RuneSchemaModEvaluateFn)(void* providerContext,
    const void* immutableResource,const double* input,uint32_t inputCount,uint64_t generation,
    double* result,uint32_t resultCapacity,uint32_t* resultCount,char* error,uint32_t errorCapacity);
// Core drains users first; this callback runs on the game thread before DLL shutdown.
typedef void (RS_MOD_CALL *RuneSchemaModReleaseResourceFn)(void* providerContext,const void* immutableResource);

typedef struct RuneSchemaModProviderV1 {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char* Id;
    const char* ResourceType;
    uint32_t InputCount;
    uint32_t MaxResultCount;
    RuneSchemaModEvaluateFn Evaluate;
    RuneSchemaModReleaseResourceFn ReleaseResource;
    void* Context;
} RuneSchemaModProviderV1;

typedef struct RuneSchemaModResourceV1 {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char* Id;
    uint32_t DataVersion;
    uint64_t Generation;
    const void* Value;
} RuneSchemaModResourceV1;

// Obtain only through the optional size-checked host GetModApi extension.
// OwnerContext is supplied by core and binds the enabled physical parent mod.
typedef struct RuneSchemaModApiV1 {
    uint32_t StructSize;
    uint32_t ApiVersion;
    void* OwnerContext;
    int32_t (RS_MOD_CALL *RegisterProvider)(void* owner,const RuneSchemaModProviderV1* provider,uint64_t* handle);
    // Success transfers release responsibility to core; failure leaves it with the provider.
    int32_t (RS_MOD_CALL *RegisterResource)(void* owner,uint64_t provider,const RuneSchemaModResourceV1* resource,uint64_t* handle);
    int32_t (RS_MOD_CALL *ReleaseResource)(void* owner,uint64_t resource);
    // Optional API 1 extension. Check StructSize and the pointer. Game thread, host only.
    // Channel is the local name from this mod's registered DataRelay. Payload is copied.
    int32_t (RS_MOD_CALL *PublishNetworkSnapshot)(void* owner,const char* channel,const char* payload,uint32_t bytes);
} RuneSchemaModApiV1;

#ifdef __cplusplus
}
#endif
