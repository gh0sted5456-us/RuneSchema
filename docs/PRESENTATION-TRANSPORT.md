# RuneSchema Presentation Transport

RuneSchema can carry small, authoritative presentation events from a server to every client without loading a mod's audio or visual assets on a dedicated server. RuneSchema transports semantic metadata only. The mod remains responsible for choosing cues and playing them locally.

## Registry declaration

Place a consumer on an `AudioPresentation` or other supported registry entry:

```json
{
  "Id": "audio_transport",
  "Kind": "AudioPresentation",
  "Consumer": {
    "Connection": "audio",
    "Class": "/YourMod/Networking/BPC_YourAudioConsumer.BPC_YourAudioConsumer_C",
    "Function": "OnRuneSchemaPresentation",
    "AllowedKeys": ["cue.one", "cue.two"]
  }
}
```

The registry owner becomes `PluginId`. A consumer class must be a cooked Actor Component Blueprint. One owner and connection may be declared by several entries only when they use the same class and function; RuneSchema merges their allowed keys. A route may contain at most 256 keys, and one merged manifest may contain at most 128 routes.

## Client consumer

Create this public Blueprint event or function on the declared component:

```text
OnRuneSchemaPresentation(
    PluginId: String,
    Connection: String,
    EntityId: String,
    Payload: String,
    Revision: Int64
)
```

RuneSchema creates one non-replicated consumer component on the client's GameState. Dedicated servers return before resolving or loading this class.

## Authority publisher

On the authoritative GameState, find `/RuneSchema/Networking/Extensions/BPC_RuneSchemaPluginPresentation.BPC_RuneSchemaPluginPresentation_C` and call:

```text
MulticastRuneSchemaPluginPresentation(
    PluginId: String,
    Connection: String,
    EntityId: String,
    Payload: String,
    Revision: Int64
)
```

`PluginId` must match the registry owner. `Connection` must match the consumer declaration. `EntityId` is an opaque, stable identifier chosen by the mod. `Revision` must increase for each plugin, connection, and entity tuple.

The payload must be a JSON object no larger than 4 KiB and contain an allow-listed string `key`. Asset paths such as `/Game/...` and `/Script/...` are rejected. Send semantic values such as cue keys, position, pitch, volume, and a server timestamp.

```json
{"key":"cue.one","state":"oneshot","position":[0,0,0]}
```

For a durable loop, publish `{"key":"cue.two","state":"start","loop":true}`. Publish the same key with `"state":"stop"` to remove it. RuneSchema retains the original payload unchanged, includes active loops in world-state replay, deduplicates stale revisions, and records client acknowledgements. Include the authoritative start time in the payload when the consumer needs playback offset after resync.

RuneSchema does not resolve `EntityId` to an Actor. Use an authoritative position in the payload for spatial one-shots. A mod that needs attachment or tracking should map its own stable replicated performer identifier on the client; UObject paths must not be sent.

## General mod-data relay (experimental)

The separate `mod.data` channel uses the existing cooked player bridge's
native server request and owner-targeted client notification RPCs. Presentation
cues above keep their existing restrictions. Matching updated core and Lua
helper are required; live Steam/Game Pass/dedicated-server round trips have
not yet been verified. This is not compatibility with an unmodded endpoint.

An enabled mod registers a channel in its normal `registry/*.jsonc` declaration
(the folder/accepted cooked registry determines the owner; a payload cannot):

```json
{
  "SchemaVersion": 1,
  "Entries": [{
    "Id": "appearance_data",
    "Kind": "DataRelay",
    "Relay": {
      "Channel": "appearance",
      "Version": 1,
      "MaxBytes": 65536,
      "ClientPublish": true,
      "Broadcast": true,
      "Snapshot": true
    }
  }]
}
```

For owner `RSHC`, the full channel is `RSHC:appearance` (case-sensitive).
Version defaults to 1, maximum bytes to 65,536; the three policy flags default
to false. Snapshot requires Broadcast. ClientPublish explicitly permits a
remote player to publish their **own** opaque session state. Broadcast opts
into sharing it with every subscriber on that server: **do not put private
profile tags, credentials or permission grants in these channels**. Private
ACL-based targeting is not implemented. This is separate from native
item/building registration and never renumbers their network indexes.

### Lua client API

Nested RuneSchema Lua packages inherit `RuneSchemaRelay`. A standalone UE4SS
mod can optionally load the same helper with `pcall(dofile,
"Mods/RuneSchema/scripts/mod-data-relay.lua")`; absence must leave its existing
standalone behavior intact. Open only channels declared on the server and
supported by your local mod:

```lua
local relay = RuneSchemaRelay
relay.Open("RSHC:appearance", 1, function(sender, data, sequence)
    -- sender is an opaque server-assigned session peer, not a Steam ID.
    -- Validate/decode data using your mod's schema; never execute it.
end)
relay.Send("RSHC:appearance", serialized_look)
-- Explicit replay of latest snapshots if local state was lost:
relay.Resync("RSHC:appearance")
-- relay.Status[channel] is a transport receipt, not gameplay success.
-- relay.LastError records helper/remote rejection details.
```

The helper waits for a locally controlled player's bridge, subscribes,
paces outgoing RPCs at four per second, assembles incoming chunks and verifies
CRC32 before the callback. Shared Lua states reuse one helper/scheduler.
Separate Lua states share the server's aggregate eight-RPC/second peer limit;
rate rejection does not automatically retry or certify delivery. A failed
transfer can be sent again with a new sequence; `Resync` asks for retained
accepted snapshots. Receipts confirm packet acceptance, not that another mod
applied the completed data. Callbacks run on the game thread.

### Native transport and limits

The public envelope is `Channel="mod.data"`, `EntityId=<owner:channel>` and
`ActionKey="subscribe"|"resync"|"chunk"` on
`ServerRequestRuneSchemaAction(Channel, EntityId, ActionKey, Payload, Revision)`.
Subscribe/resync payloads contain `{"version":1}`. The helper implements the
version-1 chunk format; native consumers can observe the matching
`ClientRuneSchemaNotification`. Payload paths are opaque data, not permission
to load assets, invoke reflection, create definitions, or edit inventory.
The sender comes from the owning native component's serial-validated identity;
any claimed sender in an upload is ignored. CRC32 detects corruption, not
malicious peers; native RPC ownership and server policy provide authority.

Limits: 64 channels, 64 session peers; 1 KiB raw chunks in at most 4 KiB
JSON envelopes; 64 KiB completed messages; 30-second assembly deadlines; four
incomplete transfers per sender; 2 MiB aggregate incoming reservations;
2 MiB latest-snapshot storage; 4 MiB conservative outbound delivery budget.
The server drains at most four chunks each 250 ms, rejects stale completed
sequences and enforces per-peer rate/byte limits. Oversized or full-capacity
operations fail explicitly, not with unbounded buffering. Server-side native
services can receive complete accepted data through `ModDataReceived`; a
public dedicated-server Lua publisher API is not part of this first version.
World reset clears transfers/snapshots/subscriptions. Native component teardown
removes that peer's retained data and queued deliveries; resubscription replays
current accepted snapshots to a late joiner.

## Packaging

RuneSchema does not select individual PAKs by client/server role. Ship one download with clearly separated `Client` and `Server` installation folders:

- Shared gameplay content and the registry declaration go to both client and server.
- Audio, visuals, and the consumer component go only to the client package.
- The dedicated-server package must have no hard imports of client presentation assets.

The RuneSchema release package supplies the cooked presentation bridge. Custom consumers must be cooked with Unreal Engine 5.6.1. The RuneSchema build verifies the adapter source contract and confirms that the presentation bridge asset exists in the shipped networking PAK.
