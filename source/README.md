# RuneSchema 0.7.5.28

RuneSchema is a UE4SS runtime for self-contained **RuneScape: Dragonwilds**
content mods.

It loads JSON/JSONC definitions, connects them to cooked assets, and provides
the multiplayer/runtime bridges used by the loader system.

## Documentation

Start here:

- [AUTHORING-GUIDE.md](AUTHORING-GUIDE.md) — installation, mod layout, order,
  cooked assets, plugins, mappings, multiplayer, logs, and testing.
- [LOADER-WALKTHROUGHS.md](LOADER-WALKTHROUGHS.md) — loader overview.
- [CHARACTER-CREATION-AUTHORING.md](CHARACTER-CREATION-AUTHORING.md) —
  character-menu data and replay behavior.
- [REGISTRY-PATCHING.md](REGISTRY-PATCHING.md) — transactional registry/DataTable
  patches.
- [COMPATIBILITY-BACKBONE.md](COMPATIBILITY-BACKBONE.md) — storefront,
  mappings, native bindings, and plugins.
- [GAMEPASS-SAVE-SYSTEM.md](GAMEPASS-SAVE-SYSTEM.md) — Xbox Game Save layout and
  provider-safe cleanup.
- [STARTUP-PERFORMANCE.md](STARTUP-PERFORMANCE.md) — measured startup stages.
- [MANUAL-SAVE-RECOVERY.md](MANUAL-SAVE-RECOVERY.md) — backup-first recovery.
- [API-REFERENCE.md](API-REFERENCE.md) — native plugin API.
- [BUILDING-CLONING-FMODEL-AUDIT.md](BUILDING-CLONING-FMODEL-AUDIT.md) and
  [raw/BASE-BUILDER-IMPORT.md](raw/BASE-BUILDER-IMPORT.md) — building and
  imported-assembly notes.

Older audit pages record implementation decisions. They do not override the
current authoring guide or loader references.

## Build

Run:

```text
..\build\build.bat -Clean
```

Outputs:

- `dist\RuneSchema-0.7.5.28-Universal.zip`
- `RuneSchema-0.7.5.28-Core.zip`
- verified Steam/GOG UE4SS runtime ZIP
- verified Game Pass/WinGDK UE4SS runtime ZIP

The universal DLL detects the storefront at runtime.

Helpy is built once as a storefront-neutral RuneSchema API client.

## Updating an install

Release archives include an empty `RuneSchema\mods` directory.

Extract an update over the existing RuneSchema directory. Do not delete the
directory first.

Keep user data:

- mod folders;
- load order;
- settings;
- the small ownership snapshot.

The ownership snapshot stores identities and owners only. It does not store or
restore character inventory, quantities, progress, or other save state.

## Mappings

An optional current `.usmap` may be placed in:

- `RuneSchema\dlls\mappings`
- the UE4SS root
- `ue4ss\mappings`

RuneSchema fingerprints the selected map at startup. It parses the map only
when the mapping service is queried.

Mapping differences are diagnostic. Live reflection validates mutations.

## Startup note

Journal finalization now uses one-pass reference indexes and cached journal
objects.

Current 0.7.5.28 journal-finalization reference:

- Game Pass: about **410 ms**, down from about **8.65 s**
- Steam/GOG: about **306 ms**

Use [STARTUP-PERFORMANCE.md](STARTUP-PERFORMANCE.md) when investigating gaps
between log lines. UE4SS object construction, Unreal readiness, and front-end
viewport creation also add normal startup pauses.

Based on the original RuneSchema 0.6.0 from Snorkles. Maintained by members of
the RSDW Modding Community. PalSchema foundation by Okaetsu.
