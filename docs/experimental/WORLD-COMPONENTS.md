---
channel: experimental
promote_to: WORLD-COMPONENTS.md
---

!!! warning "Private experimental core · validation in progress"
    This contract is under local native testing. The published 0.8.1.0 and controller-recalibration downloads do not implement it. Use only the matching test core supplied with its verification record.

# Cooked world components

An enabled mod can declare a cooked ActorComponent that RuneSchema enrolls on
its local gameplay GameState. The same declaration runs on dedicated servers,
listen hosts and clients, including a late-joining client's new GameState.
Native lifecycle validation is still pending; parser tests alone do not prove
those execution paths.

## Declaration

Use the existing [loose or cooked registry declaration](COOKED-PAK-REGISTRY-MANIFEST.md).
A loose document belongs in `mods/<Owner>/registry/10-world.jsonc`:

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "WorldComponents": [
      "/Game/Mods/Example/BP_WorldController.BP_WorldController_C"
    ]
  }
}
```

The enabled folder and registry owner must be `Example` for that path.
`/<Owner>/...` mounts are also accepted. Each path must name an owned cooked
class ending in `_C`. Cross-owner paths, duplicates, native engine classes,
abstract classes and non-ActorComponent types are rejected.

A cooked `DA_RuneSchemaRegistry_<Owner>` or `DA_Registry_<Owner>` can contain
exactly the same JSON string. No mod-specific DLL or Lua startup script is
needed. Do not also bootstrap the same controller through a ModActor or another
loader.

## Ownership and lifetime

- Components must have **Replicates disabled** in their cooked defaults and
  remain nonreplicated. Each peer owns its own local controller. This lane adds
  no RPC, state synchronization or custom network authority.
- One compatible instance is accepted per declared class and GameState;
  existing instances are reused. Ambiguous multiple instances are reported,
  without deleting components belonging to other code. A controller that removes
  itself in BeginPlay is not repeatedly recreated on that GameState.
- RuneSchema creates new instances through the existing reflected native
  `AddComponentByClass` / `FinishAddComponent` path and marks them transient.
  Existing component flags are preserved. Components retain their cooked tags,
  tick/timer configuration, BeginPlay and EndPlay behavior.
- The GameState owns each new component. No strong world reference or recurring
  discovery loop is retained. Completed native map loads and actor BeginPlay
  events resolve the active gameplay GameState; a weak state cache avoids
  repeating discovery after enrollment. This also covers GameState overrides
  that do not call the base BeginPlay hook. A one-time catch-up handles an
  already-created GameState. Clients whose GameState arrives after map loading
  are retried by subsequent actor startup events, without a recurring timer.
- Frontend and non-gameplay maps are excluded. GameState ownership does not
  provide player-controller ownership, input capture, a HUD or persistence.
- Change declarations or enabled mods between game sessions. Hot removal is
  outside this contract. The mod owns its save files, timers, visual cleanup
  and any optional communication with other mods.

A failed optional component reports `RS-WORLD-006`; world loading and other
registrations continue. Keep the class path and original exception when testing.

## Acceptance checks

Before distributing a mod that uses this lane, verify startup and repeated
entry, one instance per peer, a dedicated server plus late-joining client,
return-to-menu/world travel, EndPlay cleanup, and behavior with the mod disabled.
Test those paths in the actual game, not only an editor standalone world.

The native ownership and completion APIs are documented by
[Epic's Actor reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor).
