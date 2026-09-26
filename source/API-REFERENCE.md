# RuneSchema plugin API 1

The public C ABI is declared in
[`raw/include/Runtime/RuneSchemaPluginApi.h`](raw/include/Runtime/RuneSchemaPluginApi.h).
It lets optional native plugins register capabilities and JSON services, call
other services, and use a small set of UE4SS object helpers.

RuneSchema's semantic version and the API version are separate. A plugin can
target RuneSchema 0.7.5.28 while still using API 1.

## Files and exports

A native plugin uses this layout:

```text
RuneSchema/plugins/My.Plugin/
├─ plugin.json
├─ dll/
│  └─ My.Plugin.dll
├─ paks/                         optional
│  └─ PackageName/
│     ├─ PackageName.pak
│     ├─ PackageName.ucas
│     └─ PackageName.utoc
└─ settings/                     plugin-owned
```

## `plugin.json`

The loader recognizes this manifest contract:

```json
{
  "SchemaVersion": 1,
  "Id": "Example.Plugin",
  "Name": "Example Plugin",
  "Version": "1.0.0",
  "ApiVersion": 1,
  "BuiltForRuneSchema": "0.7.5.28",
  "EntryPoint": "Example.Plugin.dll",
  "Enabled": true,
  "ConsoleMessage": "Example plugin loaded.",
  "Capabilities": ["example.ready"],
  "Connections": ["example.content"],
  "Dependencies": {"Another.Plugin": "1.0.0"}
}
```

| Field | Required | Contract |
|---|---|---|
| `SchemaVersion` | yes | Must be `1`. |
| `Id` | yes | Unique token, at most 96 characters. |
| `Version` | yes | Plugin-owned version token. |
| `Name` | no | Display name; defaults to `Id`, maximum 128 characters. |
| `ApiVersion` | no | Defaults to API 1. The DLL descriptor is authoritative for safe ABI calls. |
| `BuiltForRuneSchema` | no | Informational compatibility version; never a semantic-version load gate. |
| `EntryPoint` | no | DLL filename directly under `dll/`. Omit for a content-only plugin. |
| `Enabled` | no | Manifest default; defaults to true. `plugins.txt` can still disable it. |
| `ConsoleMessage` | no | Single-line message, maximum 512 characters. |
| `Capabilities` | no | At most 64 unique-style tokens. A native plugin must register every declared capability during initialization. |
| `Connections` | no | At most 32 content/bridge tokens published after a successful load. |
| `Dependencies` | no | Object of plugin ID to expected version, at most 32 entries. Missing or differently versioned optional dependencies are reported without stopping the plugin attempt. |
| `Required` | legacy | Accepted for compatibility but does not override an explicit disable. Plugins remain optional to RuneSchema core. |

`PackagedRoot` is rejected. PAKs use the fixed
`paks/<PackageName>/<same-name>.pak/.ucas/.utoc` layout, and every container
triplet must be complete. The catalog rejects symlinks inside `paks/`.

`plugins.txt` contains one `Plugin.Id: 1` or `Plugin.Id: 0` entry per line.
It controls enablement and deterministic order. Dependencies refine that order;
cycles fall back to manifest order. A DLL failure does not prevent a complete
PAK triplet from being discovered and mounted.

Required DLL exports:

```cpp
extern "C" __declspec(dllexport)
const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept;

extern "C" __declspec(dllexport)
int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(
    const RuneSchemaHostApi* host, void** instance) noexcept;

extern "C" __declspec(dllexport)
void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void* instance) noexcept;
```

Optional exports:

```cpp
extern "C" __declspec(dllexport)
int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUiInit(void* instance) noexcept;

extern "C" __declspec(dllexport)
int32_t RS_PLUGIN_CALL RuneSchemaPlugin_OnUnrealInit(void* instance) noexcept;
```

Do not allow a C++ exception to cross any export or callback boundary.

## Load sequence

1. RuneSchema reads `plugin.json` and `plugins.txt`.
2. Complete PAK triplets are discovered independently from the DLL.
3. RuneSchema loads the DLL and resolves the three required exports.
4. `RuneSchemaPlugin_Query` returns the descriptor.
5. RuneSchema checks descriptor size, API version, ID, and version pointer.
6. `RuneSchemaPlugin_Initialize` receives the host table and returns an
   instance pointer.
7. The plugin must register every capability declared by its manifest.
8. RuneSchema calls the optional UI and Unreal phases when those host phases
   occur.
9. RuneSchema calls `RuneSchemaPlugin_Shutdown` before unloading the DLL.

A semantic-version difference is reported but does not stop initialization.
An API/ABI mismatch stops the native DLL because calling it is unsafe. Its PAK
content remains separate.

## Constants

### `RUNESCHEMA_PLUGIN_API_VERSION`

Current value: `1`.

Set both `RuneSchemaPluginDescriptor::ApiVersion` and the expected host API to
this value.

### `RUNESCHEMA_PLUGIN_MAX_MESSAGE`

Maximum service request or response size: 16 MiB, including the terminating
null byte where applicable. Plugins should use much smaller messages for
normal runtime work.

## Result codes

| Name | Value | Meaning |
|---|---:|---|
| `RS_PLUGIN_OK` | 0 | Operation completed. |
| `RS_PLUGIN_INVALID_ARGUMENT` | 1 | Pointer, token, size, or JSON contract was invalid. |
| `RS_PLUGIN_INCOMPATIBLE_API` | 2 | Host and plugin API versions differ. |
| `RS_PLUGIN_DUPLICATE` | 3 | Another owner already registered the name. |
| `RS_PLUGIN_NOT_FOUND` | 4 | Requested service was not registered. |
| `RS_PLUGIN_BUFFER_TOO_SMALL` | 5 | Read `responseSize`, allocate, and call again. |
| `RS_PLUGIN_FAILED` | 6 | Callback or host operation failed. |

## Log levels

| Name | Value | UE4SS level |
|---|---:|---|
| `RS_LOG_INFO` | 0 | Normal |
| `RS_LOG_WARNING` | 1 | Warning |
| `RS_LOG_ERROR` | 2 | Error |

## `RuneSchemaPluginDescriptor`

```cpp
struct RuneSchemaPluginDescriptor {
    uint32_t StructSize;
    uint32_t ApiVersion;
    const char* Id;
    const char* Name;
    const char* Version;
};
```

- `StructSize`: `sizeof(RuneSchemaPluginDescriptor)`.
- `ApiVersion`: `RUNESCHEMA_PLUGIN_API_VERSION`.
- `Id`: exact manifest ID. It uses letters, numbers, `.`, `_`, or `-`.
- `Name`: display name.
- `Version`: plugin semantic version. It does not have to equal RuneSchema's
  version.

The descriptor and its strings must remain valid until the DLL is unloaded.

## `RuneSchemaHostApi`

Check `StructSize` and `ApiVersion` before reading function pointers. The table
remains owned by RuneSchema.

### `Log`

```cpp
void Log(uint32_t level, const char* pluginId, const char* message);
```

Writes one plugin-scoped UE4SS log entry. `pluginId` should match the active
plugin ID. The host substitutes safe text for null strings.

### `RegisterCapability`

```cpp
int32_t RegisterCapability(const char* pluginId, const char* capability);
```

Registers a unique capability token during plugin initialization. The active
plugin ID must match `pluginId`. Register all capabilities declared by the
manifest or initialization is rejected.

Returns `RS_PLUGIN_DUPLICATE` when another plugin owns the token.

### `RegisterService`

```cpp
int32_t RegisterService(const char* pluginId, const char* service,
    RuneSchemaServiceFn callback, void* context);
```

Registers a unique JSON service. `callback` receives the saved `context`.
Registration is allowed during initialization while the plugin is the active
owner. Service names use the same token rules as plugin IDs.

The callback signature is:

```cpp
int32_t Callback(void* context, const char* requestJson,
    char* responseJson, uint32_t responseCapacity, uint32_t* responseSize);
```

Set `responseSize` to the required byte count, including the null terminator.
Return `RS_PLUGIN_BUFFER_TOO_SMALL` when the supplied buffer is null or too
small. A service may accept a null response buffer when it has no response.

### `CallService`

```cpp
int32_t CallService(const char* callerId, const char* service,
    const char* requestJson, char* responseJson,
    uint32_t responseCapacity, uint32_t* responseSize);
```

Routes JSON to a registered service. Use two calls for a response:

```cpp
uint32_t size = 0;
auto result = host->CallService(id, service, request, nullptr, 0, &size);
if (result != RS_PLUGIN_BUFFER_TOO_SMALL && result != RS_PLUGIN_OK) return result;
std::string response(size, '\0');
result = host->CallService(id, service, request, response.data(), size, &size);
```

The router copies the service record before invoking it, so the global service
lock is not held inside the callback. A plugin must still protect its own
state.

### `FindObject`

```cpp
void* FindObject(void* objectClass, void* objectPackage,
    const wchar_t* name, int32_t exactClass);
```

Calls RuneSchema's UE4SS-compatible object lookup. Class and package may be
null when the underlying lookup permits it. The returned `UObject*` is
non-owning and must be validated again before later use.

### `ForEachObjectOfClass`

```cpp
int32_t ForEachObjectOfClass(void* objectClass, int32_t includeDerived,
    uint64_t excludeFlags, uint32_t exclusionInternalFlags,
    RuneSchemaObjectVisitorFn visitor, void* context);
```

Builds a UE4SS object list for the class, then calls `visitor(context, object)`.
Return `RS_PLUGIN_OK` from the visitor to continue. Any other result stops
enumeration. The host function returns an API result, not the visitor's final
value.

Use object enumeration on the game thread and do not retain raw object pointers
without an Unreal lifetime mechanism.

### `FindFieldClass`

```cpp
void* FindFieldClass(const wchar_t* name);
```

Resolves an Unreal `FFieldClass` by name through RuneSchema's compatibility
layer. Returns null when the binding or class is unavailable.

### `IsFieldA`

```cpp
int32_t IsFieldA(void* field, void* fieldClass);
```

Returns `1` when the field is an instance of the requested field class and `0`
for false, invalid input, or an unavailable binding.

### `ResolveBinding`

```cpp
void* ResolveBinding(const char* callerId, const char* binding);
```

Returns a core-owned, allow-listed native binding after its provider has
validated the address. `callerId` must be a valid plugin token. Returns null
when the binding is unavailable or not exported. Current binding names and
their providers can be queried through `runeschema.bindings`.

This function is an append-only API 1 extension. An older plugin sees the same
prefix of `RuneSchemaHostApi` and remains compatible. A plugin that uses this
field must first check:

```cpp
if (host->StructSize < offsetof(RuneSchemaHostApi, ResolveBinding)
        + sizeof(host->ResolveBinding))
    return RS_PLUGIN_INCOMPATIBLE_API;
```

The returned pointer is non-owning. Do not retain it across core shutdown and
call it only with the exact documented native signature.

## Core services

Call these through `RuneSchemaHostApi::CallService`.

### `runeschema.discovery`

Request:

```json
{}
```

Response fields:

- `ApiVersion`
- `Services`: registered service names and owners
- `Capabilities`: capability names and owners
- `Connections`: active cooked/plugin connection names and owners
- `MappingBackbone`: mapping path, size, fingerprint, and lazy-index status

Discovery does not parse the USMAP.

### `runeschema.mapping`

The mapping service is optional-data aware. Every response contains `Ok` or an
`Error`. A missing mapping does not disable other services.

Status without parsing:

```json
{"Action":"Status"}
```

Describe one type. Full PPTH paths are preferred when short names are
ambiguous:

```json
{"Action":"DescribeType","Type":"/Script/Dominion.SomeStruct"}
```

Check one property:

```json
{"Action":"HasProperty","Type":"SomeStruct","Property":"PowerLevel"}
```

Search type names or paths. `Limit` is clamped to 1–100:

```json
{"Action":"SearchTypes","Query":"BuildingPiece","Limit":25}
```

Explicitly parse the mapping:

```json
{"Action":"Warm"}
```

`DescribeType`, `HasProperty`, `SearchTypes`, and `Warm` may trigger the first
parse. Parsing is never started by discovery or ordinary game startup. The
parser accepts the uncompressed UE4SS USMAP format through version 4, applies
file/count/time limits, and caches up to 128 requested type descriptions in a
2-MiB fingerprint-keyed cache.

USMAP results are advisory. Plugins must validate live objects before writes.

### `runeschema.tools`

Helpy uses this core service.

| Action | Required fields | Result |
|---|---|---|
| `Read` | optional `Revision`, `Category` | Tool state and changed snapshot. |
| `Submit` | `Request` object | `Accepted` Boolean. |
| `Completed` | `RequestId` | Completion receipt when found. |
| `Search` | `Query` | Prioritizes a catalog search. |
| `Cancel` | none | Requests cancellation. |

Tool requests remain subject to host authority and settings policy.

### `runeschema.bindings`

Reports native-binding availability and provider provenance without exposing
addresses in JSON:

```json
{"Action":"Status"}
```

Filter to one exported binding:

```json
{"Action":"Status","Binding":"UDataTable::Serialize"}
```

Each row contains `Binding`, `Available`, and `Source`. Provider values include
`embedded-aob`, `embedded-call-aob`, `live-vtable:<member>`, and `unresolved`.
Use the host `ResolveBinding` function when a native plugin needs the validated
address.

## Plugin services

Plugins may publish their own services. Helpy currently registers:

- `helpy.about`
- `helpy.tools.push`

Use `runeschema.discovery` instead of assuming a service exists.

## Minimal plugin

```cpp
#include "Runtime/RuneSchemaPluginApi.h"

namespace {
const RuneSchemaHostApi* Host = nullptr;
RuneSchemaPluginDescriptor Descriptor{
    sizeof(RuneSchemaPluginDescriptor),
    RUNESCHEMA_PLUGIN_API_VERSION,
    "Example.Plugin",
    "Example Plugin",
    "1.0.0"
};
}

extern "C" __declspec(dllexport)
const RuneSchemaPluginDescriptor* RS_PLUGIN_CALL RuneSchemaPlugin_Query() noexcept
{
    return &Descriptor;
}

extern "C" __declspec(dllexport)
int32_t RS_PLUGIN_CALL RuneSchemaPlugin_Initialize(
    const RuneSchemaHostApi* host, void** instance) noexcept
{
    if (!host || !instance || host->StructSize < sizeof(RuneSchemaHostApi)
        || host->ApiVersion != RUNESCHEMA_PLUGIN_API_VERSION)
        return RS_PLUGIN_INCOMPATIBLE_API;
    Host = host;
    *instance = nullptr;
    return host->RegisterCapability(Descriptor.Id, "example.ready");
}

extern "C" __declspec(dllexport)
void RS_PLUGIN_CALL RuneSchemaPlugin_Shutdown(void*) noexcept
{
    Host = nullptr;
}
```

The manifest must declare `example.ready` in `Capabilities` or omit capability
declarations. Manifest declarations are checked after initialization.

## Lifetime and safety rules

- Copy any host response that must survive the call.
- Keep descriptor strings static.
- Keep service callback/context storage alive until shutdown.
- Do not unload or replace the plugin DLL while callbacks are active.
- Do not throw through the ABI.
- Treat all Unreal pointers as non-owning.
- Check `RuneSchemaHostApi::StructSize` before using append-only API fields.
- Use the game thread for Unreal traversal and mutation.
- Validate reflected type, property, array dimension, and flags before writes.
- Treat USMAP data as a preflight index, never as write authorization.
- Return bounded JSON and validate input before acting on it.
