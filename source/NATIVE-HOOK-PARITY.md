# Native hook support

RuneSchema uses one `main.dll` with separate Steam/GOG and Game Pass/WinGDK
native lanes.

A native call is never reused across storefronts just because the byte pattern
looks similar.

## Activation

A hook is enabled only when these checks agree:

1. storefront lane;
2. PE timestamp and image size;
3. full bytes at every hook site;
4. full bytes at every nonzero resume address;
5. callback register contract.

If validation fails, that capability stays off. Other loaders, mods, plugins,
and core services continue.

## Support matrix

| Capability | Steam/GOG | Game Pass 100.4.0.0 | Fallback |
| --- | --- | --- | --- |
| Core engine signatures | Verified profile | Verified WinGDK profile | UE4SS/reflection where available |
| Object enumeration | Verified native `TArray` routine | UE4SS hash tables/object array; native callback iterator rejected | Yes |
| Shadowveil equipment actions | Five verified sites | Five verified sites at `0x6E48153`, `0x6E48171`, `0x6E48224`, `0x6E48192`, `0x6E481BA` | Only Shadowveil binding is inactive |
| Surge/Dash equipment behavior | Eight Steam sites verified against PE `0x6BAC8379`, image `0x0DDEC000` | Incomplete trace; inactive | Windstep and reflected `GrantedEffects` still load |
| Journal/lore registration | Reflected registry plus validated Steam hierarchy routines | Reflected registry plus isolated WinGDK hierarchy/category routines | Skip affected hierarchy placement |
| Journal native JSON save cleanup | Verified Steam adapter | Reader/writer traced; helper ABI incomplete; inactive | Journal content and unlock delivery continue |
| Appearance event bridge | Five-site Steam contract | WinGDK wearable-mesh event verified; other sites inactive | Consumer reports unavailable |
| Merchant stock refresh | Reflection first; native cache optional | Reflection first; native cache optional | Scheduled replicated-state refresh |
| Quest, dialogue, event, time-of-day, spawn | Reflected `UFunction` / RuneSchema API | Same | Feature-scoped warning |

## Game Pass evidence

Audited WinGDK build:

- version: `100.4.0.0`
- timestamp: `0x9924253F`
- image size: `0x0DB11000`

Journal save routines were traced at:

- writer: `0x6EA3590`
- reader: `0x6EA3800`

They are evidence only. RuneSchema does not use the native save adapter until
the JSON helpers and ownership rules are verified.

Validated journal hierarchy/category entry points:

- hierarchy insert: `0x6ED8FD0`
- category 1: `0x713DD80`
- category 2: `0x713DDF0`
- category 3: `0x713DE60`
- builder/layout witness: `0x6EA5DA0`

The three category functions are separate one-argument WinGDK entry points.
Steam keeps its single category dispatcher.

All five WinGDK patterns must resolve uniquely in executable memory.

## Save storage

### Steam/GOG

Character JSON files live under:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved
```

They can be backed up and cleaned as ordinary files.

### Game Pass

Xbox Game Save data lives under the package `SystemAppData\wgs` tree.

RuneSchema does not rewrite that provider database as loose files. Cleanup runs
through the game-owned payload while the provider is active.

RuneSchema's ownership ledger is separate under the package
`LocalState\RSDragonwilds\Saved\RuneSchema` tree.

## Rejected WinGDK candidate

The earlier `0x76C86F0` candidate was rejected after live tracing and a crash
dump review.

It inserts a different map layout:

- 64-byte key;
- 8-byte value.

Journal hierarchy records use:

- 40-byte soft-object key;
- 28-byte value.

The mismatch caused an invalid native copy. The verified `0x6ED8FD0` routine
matches the journal builder and record layout.

## Update policy

A game update normally changes PE identity, so native capabilities remain
inactive until a new profile is traced.

Add a new immutable profile for each verified build. Do not overwrite old
profiles or loosen byte validation.
