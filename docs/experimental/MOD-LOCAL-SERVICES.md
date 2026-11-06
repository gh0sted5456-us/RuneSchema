---
channel: experimental
promote_to: MOD-LOCAL-SERVICES.md
---

!!! warning "Interface v1 frozen; implementation in progress"
    This is a new contract, not an API in the installed core or existing downloads.
    Do not claim template or compute integration until a matching native candidate
    passes its actual Blueprint and platform tests.

# Mod-local templates and compute

The mod owns its UI, schema, calculations, numeric resources and native SaveGame
fallback. RuneSchema provides an optional local broker. This interface grants no
network authority, arbitrary file paths or UObject property mutation.

## Declaration and packaging

An enabled mod declares its exact cooked ActorComponent classes in its existing
loose or cooked registry JSON. The component must belong to the actual local
PlayerController and share its world. Frontend drafts do not need a pawn or GUID.

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "LocalServices": [
      {"Version": 1, "Id": "templates", "Class": "/Game/Example/Core/BPC_Templates.BPC_Templates_C", "Templates": true},
      {"Version": 1, "Id": "compute", "Class": "/Game/Example/Core/BPC_Compute.BPC_Compute_C", "ComputeProviders": ["Example.ShapeMath/Check180"]}
    ]
  }
}
```

The physical enabled folder and declaration owner are `Example`. Class paths
must belong to `/Example/`, `/Game/Example/` or `/Game/Mods/Example/`; namespace
and live component ownership are both checked. Classes and entry IDs are unique
per owner. Local calls are impure, non-RPC Blueprint functions with exact named
pins and no return-value pin. Original bodies return unavailable defaults.

An optional native provider stays inside that same mod:

```text
mods/Example/
  paks/Core/<complete pak/ucas/utoc trio>
  plugins/Example.ShapeMath/
    plugin.json
    dll/Example.ShapeMath.dll
```

The new mod-owned plugin lane uses API 1 `RuneSchemaPlugin_Query`,
`RuneSchemaPlugin_Initialize` and `RuneSchemaPlugin_Shutdown`, with `Host: "game"`
and `EntryPoint: "Example.ShapeMath.dll"`. It is not the nested Lua loader or a
stock UE4SS `start_mod` DLL. Core derives the parent owner from its enabled mod
snapshot. Native provider IDs must remain in that owner's namespace. Disabling
or changing native modules requires restart; there is no hot DLL unload.

## Template files and typed documents

The root is the current user's Windows Known Documents folder followed by
`SavedGames/RSDragonwilds/<Owner>`. Steam and Game Pass use the same owner root.
No root, absolute path or character GUID is accepted from Blueprint. Copied and
renamed ordinary `.json` files are readable; IDs are bounded safe basenames,
and the stable list label is the filename stem. No filename prompt is required.

Each file must contain a strict UTF-8 JSON object of at most 64 KiB. Duplicate
keys, nonfinite numbers and nesting beyond 32 levels are rejected. Creating a
file publishes a flushed stage atomically under an auto-generated unique ID;
existing files are never overwritten. There is no delete/update operation in v1.

Document handles belong to the declaring owner and exact live component. At most
eight documents can remain open per component. They are not pointers and cannot
be transferred between mods or survive their component/session. Reading a file
creates an editable local document; saving it creates a new file.

All names below are exact. Inputs precede outputs; outputs are ordinary output
pins, not reference/in-out parameters. `Success` is bool, `Error` is string,
`Document` and `Ticket` are positive int64 handles. No function has a return pin.

| Function | Inputs | Outputs |
| --- | --- | --- |
| `RuneSchemaTemplateList` | none | Success; Templates: string array; Labels: string array; Error |
| `RuneSchemaTemplateRead` | TemplateId: string | Success; Document; Error |
| `RuneSchemaJsonCreate` | none | Success; Document; Error |
| `RuneSchemaJsonRead` | Document; Pointer: string | Success; Kind: byte; Number: double; Text: string; Boolean: bool; Count: int32; Error |
| `RuneSchemaJsonSet` | Document; Pointer: string; Kind: byte; Number: double; Text: string; Boolean: bool | Success; Error |
| `RuneSchemaJsonKeys` | Document; Pointer: string | Success; Keys: string array; Error |
| `RuneSchemaJsonReadNumbers` | Document; Pointer: string | Success; Values: double array; Error |
| `RuneSchemaJsonSetNumbers` | Document; Pointer: string; Values: double array | Success; Error |
| `RuneSchemaJsonReadStrings` | Document; Pointer: string | Success; Values: string array; Error |
| `RuneSchemaJsonSetStrings` | Document; Pointer: string; Values: string array | Success; Error |
| `RuneSchemaTemplateCreate` | Document | Success; TemplateId: string; Error |
| `RuneSchemaDocumentClose` | Document | Success; Error |

`Kind` is 0 Null, 1 Boolean, 2 Number, 3 String, 4 Array, 5 Object. A JSON Pointer
is at most 512 UTF-8 bytes and 32 segments. Empty Pointer addresses the root,
which must remain an object. `~0` and `~1` escape tilde and slash. Array `-` is
append-only on writes; reads cannot use it. Missing intermediate containers are
not created implicitly. Array indexes are existing entries or the exact next
index on writes, never sparse indexes. Reads never coerce types.

Bulk functions accept at most 1,024 values and enforce one homogeneous type;
number arrays contain finite doubles. A failed write leaves the document
unchanged. `Count` is the element/property count for arrays/objects and zero for
scalars. Listing is bounded to 1,024 ordinary files; excessive/unsafe directories
report an error without deleting or silently pruning anything.

## Native compute providers

The engine-free C header is `source/shared/RuneSchemaModApi.h`, API version 1.
An optional append-only `RuneSchemaHostApi.GetModApi(pluginId)` slot returns a
`RuneSchemaModApiV1` table only for an enabled mod-owned plugin during its
initialization. Check host `StructSize` before reading the slot, then the returned
table's `StructSize`/`ApiVersion` and required function pointers. An old host or
missing slot means unavailable, never another DLL copy or a private Helpy pipe.

All registration/resource-release calls run on the game thread. Core supplies
`OwnerContext`; callers do not choose their mod owner. Provider and resource IDs
are bounded ASCII tokens. The provider declares its exact input count and result
capacity, up to 256 and 1,024 doubles respectively, plus an immutable resource
type name. Each resource has its own positive data version/generation and
provider-owned immutable numeric context. It is not serialized into JSON.

`RegisterResource` transfers release responsibility only on success. Core keeps
resources alive through active calls. `ReleaseResource` retires a resource,
cancels queued work and drains active users before invoking the provider's
release callback on the game thread. Core drains all work and releases resources
before invoking plugin Shutdown or unloading the module. Providers must finish
bounded work and must not enter the engine from Evaluate.

Evaluate is serialized on one worker per provider. It receives the immutable
resource, a copied double input vector and request generation, and fills a
core-owned result buffer/error. It cannot retain those buffers. Result count is
bounded by the descriptor and all values must be finite. Error buffers hold at
most 256 UTF-8 bytes including their terminating null. This is a trusted native
plugin interface, not a sandbox for hostile DLL code or an expression evaluator.

| Blueprint function | Inputs | Outputs |
| --- | --- | --- |
| `RuneSchemaCompute` | Resource: string; Weights: double array; Generation: int64 | Accepted: bool; Ticket: int64; Error: string |
| `RuneSchemaComputePoll` | Ticket: int64 | Ready: bool; Success: bool; Generation: int64; Values: double array; Error: string |

Resource names are `<PluginId>/<ProviderId>/<ResourceId>` and must match an
allowed `<PluginId>/<ProviderId>` from the declaring component's registry row.
Request generations are positive and monotonically increase per component.
There is one active request and at most the latest pending request per component;
newer generations supersede stale previews. Resource generation changes invalidate
results. No resource/handle from another mod or component may be used.

A component may implement the local non-RPC event/function
`RuneSchemaComputeCompleted` with six input pins, in this order: Ticket: int64;
Success: bool; Generation: int64; Values: double array; Error: string;
ResourceGeneration: int64. No outputs/return pin. Core delivers it on the game
thread after checking component, world, ownership and current generations. Poll
remains available while the component is live. Delivery uses the core's pending
completion queue, not a mod actor Tick or a world-scanning timer.

A provider registers only a capability it has actually implemented and tested.
A probe evaluator and complete solver use different provider IDs; loading a DLL
alone cannot claim the complete solver or UI is ready. Network payload limits,
profile GUID storage and the owning mod's Apply/Cancel behavior are unchanged.
