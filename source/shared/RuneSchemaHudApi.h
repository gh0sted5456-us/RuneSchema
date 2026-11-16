#pragma once
#include <stdint.h>

// Optional, in-process adapter API. Locate RuneSchema.HUD.dll; never LoadLibrary
// a second copy. The UI/texture producer may live in a separate process.
#if defined(_WIN32)
#define RS_HUD_CALL __cdecl
#else
#define RS_HUD_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
enum RuneSchemaHudResult {
    RS_HUD_OK=0, RS_HUD_INVALID_ARGUMENT=1, RS_HUD_UNAVAILABLE=2,
    RS_HUD_NOT_FOUND=3, RS_HUD_BUFFER_TOO_SMALL=4, RS_HUD_FAILED=5
};
enum { RS_HUD_MAX_MESSAGE=8192 };
enum RuneSchemaHudCommand { RS_HUD_COMMAND_NONE=0, RS_HUD_COMMAND_ACTIVATE=1, RS_HUD_COMMAND_DEACTIVATE=2 };
typedef struct RuneSchemaHudApi {
    uint32_t StructSize;
    uint32_t ApiVersion;
    int32_t (RS_HUD_CALL *RegisterClient)(const char* owner, uint32_t producerPid, uint64_t* client);
    int32_t (RS_HUD_CALL *Call)(uint64_t client, const char* requestJson,
        char* responseJson, uint32_t responseCapacity, uint32_t* responseSize);
    void (RS_HUD_CALL *CloseClient)(uint64_t client);
    int32_t (RS_HUD_CALL *NextCommand)(uint64_t client, uint32_t* command);
    // Duplicates an event into the adapter's process. The adapter closes it.
    int32_t (RS_HUD_CALL *DuplicateWakeEvent)(uint64_t client, uint64_t* eventHandle);
} RuneSchemaHudApi;
typedef const RuneSchemaHudApi* (RS_HUD_CALL *RuneSchemaHudQueryFn)(void);
#ifdef __cplusplus
}
#endif
