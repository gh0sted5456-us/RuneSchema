# Compatibility

RuneSchema supports Steam/GOG and Game Pass/WinGDK with one storefront-aware runtime.

## Storefronts

### Steam / GOG

RuneSchema uses the Steam/GOG support path when the running game is the
Steam/GOG build.

### Game Pass / WinGDK

RuneSchema uses the WinGDK runtime lane. Steam-only byte patterns are not used
on Game Pass.

Xbox Game Save data is managed by the Xbox app. RuneSchema does not edit those
files as though they were Steam character files.

## Mappings

RuneSchema looks for an optional `.usmap` in these locations:

1. `Mods/RuneSchema/dlls/mappings`
2. the UE4SS root
3. `ue4ss/mappings`
4. older RuneSchema mapping locations kept for compatibility

Mappings improve field names, type information, and diagnostics. The loaded
game still decides whether a requested object or field is available.

## Plugins

Plugins are optional.

A plugin built for an incompatible RuneSchema version can be skipped without
disabling RuneSchema itself. Valid plugin PAK content can remain independent
from an optional plugin DLL.

Helpy is not required by the loader system.

## Multiplayer

Gameplay mutations remain server-owned. Clients need the cooked assets required
for anything they render.

`/registry` connects server actions with client presentation, but a registry
entry does not bypass server validation.

## Native feature fallback

Some features need a storefront-specific game function. If RuneSchema cannot
verify that function for the current game build, it leaves only that feature
off and continues loading unrelated content.

A game update may therefore temporarily affect one native feature without
breaking normal JSON authoring.

## Saves

SafeSave removes only content with recorded RuneSchema ownership.

Steam/GOG and Game Pass store saves differently, but RuneSchema applies the
same ownership and cleanup rules after the game loads a character. Do not use
Steam file-editing instructions on Xbox Game Save files.

For user recovery steps, see
[Manual Save Recovery](MANUAL-SAVE-RECOVERY.md).

For storefront detection, hook validation, WGS internals, and storefront-specific
details, see the [Developer Guide](DEVELOPER-GUIDE.md) and the
[side-by-side lane reference](STOREFRONT-LANES.md).
