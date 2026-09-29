# Compatibility

RuneSchema uses one storefront-aware runtime for Steam/GOG and Game Pass/WinGDK.

## Storefronts

At startup RuneSchema detects the running game and logs the selected native
binding lane. Steam-only native patterns are never reused on a detected WinGDK
build.

The JSON loaders are shared between storefronts.

## Multiplayer

RuneSchema content is network-mode agnostic. Authors do not add a
`Multiplayer` or single-player flag.

The server owns gameplay mutations such as inventory, purchases, quests,
spawning, buildings, AI, drops and events. Clients render replicated state and
the cooked assets installed locally.

`/registry` is for features that need an explicit server action paired with
client presentation. It does not bypass server validation.

## Saves

Steam and Game Pass store character data differently, but RuneSchema applies the
same cleanup decision at the game character-load boundary.

RuneSchema does not treat Xbox Game Save provider files as ordinary Steam JSON
files and does not edit WGS containers directly.

Safe Clean removes unresolved RuneSchema identities after active content has
registered. Vanilla and unrelated third-party identities are outside that rule.

## Mappings

`Mappings.usmap` is optional. RuneSchema uses live reflection as the final
authority and uses mappings for better names, diagnostics and tooling.

The canonical location is:

```text
Mods/RuneSchema/dlls/mappings/Mappings.usmap
```

Compatibility locations are still read for existing installs.

## Plugins

Plugins are optional. A plugin failure or version mismatch does not disable
RuneSchema core. Helpy is not required by the loader system.

## Native feature fallback

Some features depend on storefront-specific game functions. If RuneSchema cannot
verify one of those functions after a game update, only that feature is disabled
where possible.

See the [Developer Guide](DEVELOPER-GUIDE.md) for implementation details and
[Manual Save Recovery](MANUAL-SAVE-RECOVERY.md) for recovery steps.
