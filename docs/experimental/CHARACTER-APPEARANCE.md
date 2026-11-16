---
channel: experimental
promote_to: modders/CHARACTER-APPEARANCE.md
---

!!! info "Experimental · 0.8.1.0"
    This is the local appearance-transport candidate. Use its matching Core and
    complete RSNetworking PAK/UCAS/UTOC trio. Rendered multiplayer acceptance with
    a real morph provider remains required. [Experimental overview](index.md).

# Multiplayer character appearance

RuneSchema carries committed cosmetic state from a player's owned bridge to the
server and then to connected clients. The server derives the subject Actor and
native character GUID from the actual sender. Uploaded JSON cannot select a
different player, grant gameplay authority or request profile-file operations.

The appearance mod owns its UI, reversible local preview, body/head assets,
provider enrollment, schema validation and cosmetic application. Its local
profile remains separate. Receiving an appearance, reset or replay must never
write another player's data to the local companion save.

## Keep the appearance mod usable on its own

An independent UE4SS appearance mod must start without RuneSchema. Its own UI,
provider discovery, local profiles, morphs, preview, Cancel and Submit remain
available when Core is missing, incompatible or still starting. Prefer compatible
RuneSchema Core and registry services when available; make the networking adapter
optional and keep the mod's own data contract. Do not hard-link the standalone
DLL against Core, load a second copy through RuneSchema, or automatically move
existing profiles between storage backends.

RuneSchema owns its enabled registry declarations and shared network bridge.
The appearance mod owns its providers, assets, local persistence and cosmetic
application. Give each PAK and callback one loader/registration owner. A separate
UE4SS installation must not mount or start that same payload again just because
an optional RuneSchema sidecar is present.

The [shared HUD plugin](SHARED-HUD.md) exposes its own optional adapter API.
Its UI readiness is independent of appearance networking. There is currently no
public Core capability-query export for a separate UE4SS DLL.
The versioned `RuneSchemaHostApi` is supplied to Core-managed native plugins at
initialization; its `runeschema.discovery` service does not certify appearance
routes. Do not obtain private pointers or assume a folder, version label, mounted
PAK or reflected class proves Core and its route are ready.

After the owned local pawn has the exact matching bridge contract, request
`resync` for the specific enrolled key using the call below. Match the
`ClientRuneSchemaReceipt` channel, key and request revision, and require acceptance
before reporting that the server route is available. Using `all` does not verify
an individual route. This handshake requests snapshots without publishing a
cosmetic change or writing a profile; it is not a pure capability query.
Acceptance proves transport availability, not successful mesh application.

Keep rejection, timeout or missing contracts local-only. Clear readiness when
the world or owned endpoint changes, and use bounded retries at lifecycle events
instead of continuous polling. Networking needs compatible Core and the complete
matching RSNetworking PAK/UCAS/UTOC trio on participating installations. The PAK
alone is insufficient.

## Register a route

Declare this entry through the normal loose or cooked registry loader. For an
optional standalone adapter, a loose sidecar can live at
`RuneSchema/mods/JBM/registry/appearance.jsonc`:

```json
{
  "Entries": [{
    "Id": "appearance",
    "Kind": "CharacterAppearance",
    "Appearance": {"MaxBytes": 4096}
  }]
}
```

For an enabled owner folder named `JBM`, its registry key is `JBM:appearance`.
The owning mod folder determines the namespace. Neither a display name nor a
client-supplied JSON field changes it. The server must have the same route enabled;
clients without the route ignore its cosmetic data.

An independent UE4SS DLL can hook the central cooked callback below. A pure
Blueprint mod can instead add `Class` and optional `Function` inside `Appearance`:

```json
{"Class":"/JBM/Runtime/BPC_Appearance.BPC_Appearance_C",
 "Function":"OnRuneSchemaCharacterAppearance","MaxBytes":4096}
```

That registered class must be a cooked ActorComponent with the exact six-input,
local, non-RPC callback below. RuneSchema reuses or creates one nonreplicated,
tick-disabled consumer on the subject pawn. Native UE4SS consumers do not need
this component or a link against RuneSchema's DLL.

## Publish a committed change

On the game thread, find the registry component on the actual locally controlled
player pawn:

```text
/RuneSchema/Networking/BPC_RuneSchemaRegistryBridge.BPC_RuneSchemaRegistryBridge_C
```

Call its existing reliable `ServerRequestRuneSchemaAction`:

| Input | Value |
| --- | --- |
| Channel | `character.appearance` |
| EntityId | The registered key, for example `JBM:appearance` |
| ActionKey | `publish`, `reset` or `resync` |
| Payload | A UTF-8 JSON object; use `{}` for reset/resync |
| Revision | A positive int64 request revision |

Publish/reset revisions must increase for that owned endpoint and registry key.
Independent providers and automatic resync requests do not share this sequence.
RuneSchema assigns a separate monotonic **server state revision** to accepted
changes. An identical canonical payload does not generate another appearance
broadcast. A `ClientRuneSchemaReceipt` on this channel reports server acceptance
or rejection; acceptance establishes stored transport state, not successful mesh
application by every recipient.

`resync` can use `all` as EntityId. It replays current appearance snapshots to the
requesting player. `reset` retains a reset snapshot for later joiners. Disconnect,
destroyed subject and world teardown release session state; the mod republishes
its own committed profile after respawn/reconnect. This transport is not disk
persistence.

Preview slider motion stays local. JBM publishes only on explicit Submit;
Cancel, profile loading and received network state never publish. Payloads must
fit the route's limit, which defaults to 4,096 bytes. Matching updated runtimes can opt into 16 KiB through the [network capacity candidate](NETWORK-CAPACITY.md). Generic requests retain the existing
generic request object's 32 top-level fields. Put extensible controls inside a
`values` object. Versions and schemas inside the payload are owned by the mod.

## Receive the verified subject

The matching cooked RegistryBridge class contains this reliable owning-client RPC:

```text
ClientRuneSchemaCharacterAppearance(
  RegistryKey: String,
  Subject: Actor,
  CharacterGuid: String,
  Payload: String,
  Revision: Int64,
  Removed: Boolean)
```

The optional Blueprint consumer has the same inputs and defaults to the local
function name `OnRuneSchemaCharacterAppearance`. The Actor is the actual replicated
subject reference. The GUID is server-derived, with 32 uppercase hexadecimal digits;
neither field comes from the uploaded payload.

A native receiver must verify the exact central class, reliable client function,
local ownership of the receiving endpoint, declared RegistryKey and actor/world
binding before applying cosmetics. Use the supplied Subject, and reject unknown
provider IDs, body types, mesh contracts, control schemas, keys and out-of-range
or nonfinite values. Never resolve arbitrary asset paths supplied by a client.
Keep revision/deduplication state scoped to the subject and world. `Removed=true`
means restore the mod's cosmetic baseline without writing a profile.

RuneSchema replays retained snapshots when an endpoint joins or requests resync.
Its internal reliable `ServerSetRuneSchemaAppearanceSubject(Subject: Actor,
Present: Boolean)` handshake advertises client-known subjects on spawn and
withdraws them on end-play. The server replays remote subjects only after that
recipient has advertised the reference, reducing unmapped-reference delays.
The local/listen-server view can use its already-known world actors. Providers
do not need to implement this handshake. Client character-spawn events request
fresh snapshots, covering later actor relevance. An unresolved subject triggers bounded resync retries. Delivery uses
the existing engine callback and a small send budget while work is pending;
there is no separate Lua polling loop. Unavailable cooked contracts are reported
through RuneSchema's existing bridge diagnostics rather than treated as success.

## JBM provider example

The coordinated JBM payload shape is opaque to Core:

```json
{
  "schema": 1,
  "provider": "JBM.male",
  "bodyType": "male",
  "meshContract": "jbm.male.v1",
  "headMesh": "ENROLLED_HEAD_ID",
  "controlsHash": "SHA256_OF_THE_INSTALLED_CONTROL_CONTRACT",
  "enabled": true,
  "values": {"muscle": 50, "bodyFat": 50}
}
```

JBM validates the exact provider/body/mesh/control contract before changing the
character. Male, female and third-party provider profiles must not be mixed.
The values above illustrate the transport; they are not a complete provider
definition or proof that a particular morph mesh is installed.
