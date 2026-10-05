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

## Packaging

RuneSchema does not select individual PAKs by client/server role. Ship one download with clearly separated `Client` and `Server` installation folders:

- Shared gameplay content and the registry declaration go to both client and server.
- Audio, visuals, and the consumer component go only to the client package.
- The dedicated-server package must have no hard imports of client presentation assets.

The RuneSchema Universal package supplies the cooked presentation bridge. Custom consumers must be cooked with Unreal Engine 5.6.1. The RuneSchema build verifies the adapter source contract and confirms that the presentation bridge asset exists in the shipped networking PAK.
