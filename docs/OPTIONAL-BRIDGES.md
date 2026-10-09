# Optional RuneSchema bridges

RuneSchema's release package includes helpers for mods that need more
than normal JSON loading or ordinary cooked PAK mounting. They are separate so
you can use only what your mod list needs.

Starting with the next release, the single package includes all bridges and
enables them by default. There is no separate plugin-free download. For the
existing 0.7.7.5m release, choose **Universal**. Disable a bridge only when no
enabled mod depends on it.

## Quick choice

| Component | Use it when | Safe to turn off when |
| --- | --- | --- |
| RuneSchema Registry Bridge | A cooked PAK uses a `DA_RuneSchemaRegistry_*` declaration to introduce items, recipes, spells, attacks, or effects to the live game lists. | No enabled mod says it requires the shared registry bridge. |
| RSNetworking | A mod uses RuneSchema's supported multiplayer identity, world-state, dialogue-cue, or registry presentation bridge. | No enabled mod lists RSNetworking as a requirement. |
| RuneSchema.Helpy | You want the optional in-game RuneSchema helper interface. | You do not use that interface. |
| Combat fallback | A weapon mod cannot reach its cooked attack collections through the normal game-owned route and its author specifically asks for the fallback. | In almost every normal setup. It is off by default. |

The first three are plugins. Combat fallback is a RuneSchema setting, not a
PAK and not a separate download.

## Turn a plugin on or off

Open:

```text
RuneSchema/plugins/plugins.txt
```

The package starts with:

```text
RuneSchema.RegistryBridge : 1
RSNetworking : 1
RuneSchema.Helpy : 1
```

`1` means enabled and `0` means disabled. RuneSchema reads the list from top to
bottom. Restart the game after changing it.

Do not disable a bridge that an enabled mod requires. RuneSchema will not
silently replace a missing bridge with a different loading method.

## Shared registry bridge

The registry bridge is for cooked PAK content that needs an explicit path into
Dragonwilds' live gameplay lists. Mounting a PAK makes its files available, but
it does not guarantee that every new item, recipe, spell, or attack becomes a
usable game entry.

The package supplies these three matching runtime files:

```text
RuneSchema/plugins/RuneSchema.RegistryBridge/paks/RegistryBridge/
  RegistryBridge_P.pak
  RegistryBridge_P.utoc
  RegistryBridge_P.ucas
```

Keep all three together. The bridge provides the shared base at:

```text
/RuneSchema/Registry/PDA_RuneSchemaRegistryBridgeBase
```

Each content mod still cooks its own uniquely named
`DA_RuneSchemaRegistry_*` asset. The shared bridge never scans every mounted
asset, never adds duplicate entries, and never replaces another mod's table.
If the game already registered an entry, RuneSchema leaves it alone.

Authors can follow the [shared registry bridge PAK walkthrough](SHARED-REGISTRY-BRIDGE-PAK.md).

## RSNetworking

RSNetworking supplies cooked bridge objects for mods that use RuneSchema's
supported multiplayer identity, world-state, dialogue-cue, registry
presentation, or validated authority paths. It does not make an otherwise
single-player mod multiplayer-safe by itself. The mod must remain
self-contained and own the complete gameplay Blueprint; the networking PAK is
only the bridge across the boundary the game does not already cover.

For gameplay content, the server and every client should use the same mod
versions, PAKs, enabled bridges, and load order. If a mod does not name
RSNetworking as a requirement, RuneSchema does not need it for ordinary item,
recipe, station, or save cleanup work.

## Helpy

Helpy is the optional in-game helper interface. Turning it off removes that
interface without disabling RuneSchema's normal loaders, PAK mounting, registry
validation, recipe placement, or save cleanup.

## Combat fallback is separate

Combat fallback is an advanced compatibility helper for melee, ranged, or
magic registration. It is disabled by default and should stay off unless a mod
author asks you to enable a specific lane.

It does not control item registration, recipe registration, save cleanup, the
shared registry bridge PAK, or RSNetworking. Turning off combat fallback does
not turn off normal game-owned weapon relationships.

See [combat fallback settings](COMBAT-FALLBACK-SETTINGS.md) for the individual
switches and their order.

## Confirm what loaded

After restarting, open `UE4SS.log` and search for the plugin names. A healthy
RuneSchema startup reports that each enabled plugin was found and loaded. A
disabled plugin should not mount its PAKs or announce its cooked objects.

If a content mod fails after a bridge is disabled, restore the bridge to `1`,
restart the game, and check the mod author's requirements before changing any
save-cleanup or combat settings.
