---
channel: experimental
promote_to: modders/MULTIPLAYER-PLUGIN.md
---

# Multiplayer plugin

!!! info "Experimental · server-side replication tuning"
    Use matching RuneSchema Core, Helpy, and the complete RSNetworking package.
    Verify effective actor visibility and traffic in a real multiplayer session.

RuneSchema supplies the common transport for cooked Blueprint, native DLL, and
Lua mods. Its RSNetworking PAK trio supplies the replicated components and RPCs.
The native `RuneSchema.Multiplayer.dll` adds host policy and stores its overrides
in `RuneSchema/settings/multiplayer.json`. Core applies changes on the game thread.
The existing cooked bridge can still operate without the policy DLL.

## Opt in an actor

An enabled mod can add this entry to its loose `registry/*.jsonc` or cooked
`RegistryJson` declaration. Use the exact class of the mod-owned actor.

```json
{
  "SchemaVersion": 1,
  "Entries": [{
    "Id": "vehicle",
    "Kind": "MultiplayerActor",
    "ActorClass": "/Example/Actors/BP_Vehicle.BP_Vehicle_C"
  }]
}
```

A native actor class can use its exact `/Script/ExampleModule.ActorClass` path.
The registry owner supplies the namespace. The example appears as
`Example:vehicle` if the enabled mod folder is `Example`. Classes claimed by
multiple entries are withheld from tuning until their declarations are unique.

Open **Server / Co-op → Multiplayer** in Helpy on the listen or dedicated host.
Select the declaration, enable the range or update-rate override, and apply it.
The host rechecks existing instances and applies the policy to newly started
actors. **Restore defaults** removes the saved override and restores values
RuneSchema still owns. The actor must already replicate. Distance tuning skips
actors whose relevance is always-on or based on ownership.

An applied count confirms native field readback. Streaming, the active
replication driver, dormancy, and connection bandwidth can affect observed range
and update rate. Check actual arrival on a second client before relying on a
setting. Range tuning does not change interaction distance or grant authority.

## Use the shared transport

| Mod implementation | Integration |
| --- | --- |
| Cooked Blueprint | Declare routes in the cooked registry and use the corresponding cooked callbacks and RPCs. |
| Lua | Open a registered DataRelay channel with `RuneSchemaRelay.Open`. Wait for its server receipt before using `Send` or switching transport. |
| Native DLL | Use the cooked endpoints on the game thread. A Core-managed DLL can also query `runeschema.tools` with `{"Action":"MultiplayerRoute","Kind":"DataRelay","Key":"Example:state"}`. |

The query returns `Protocol` and `Declared` for the local registry. Supported
kinds are `DataRelay`, `CharacterAppearance`, and `MultiplayerActor`. For
DataRelay, use the full channel name. For the other kinds, use the registry key.
The query does not certify a remote connection. Wait for the corresponding
subscription or route-specific resync receipt before switching a standalone
mod to RuneSchema. Clear that decision when the world or owned endpoint changes.

For an authoritative native DLL, obtain `RuneSchemaModApiV1` through
`RuneSchemaHostApi::GetModApi`. Size-check the appended
`PublishNetworkSnapshot` pointer, then pass the supplied owner context, local
channel name, and payload bytes on the game thread. Declare the channel with
`Broadcast` and `Snapshot` enabled. The relay checks channel ownership, retains
the latest accepted snapshot, and sends it to subscribed clients. Clients
receive it through the existing notification callback. A mod-owned native
plugin must declare `"ServerCompatible": true` to load on a dedicated server.

Use the existing [data relay](../PRESENTATION-TRANSPORT.md) for mod data and
[character appearance route](CHARACTER-APPEARANCE.md) for cosmetic snapshots
such as JBM. Mod authors own their payload schema and standalone fallback.
