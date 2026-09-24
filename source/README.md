# RuneSchema 0.7.5.22

This revision extends absolute scale reconciliation to every RuneSchema-managed
actor path and adds appearance-only, write-once player fallbacks under
`%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\players`. See
`RELEASE-0.7.5.22.md` for the exact behavior and privacy boundary.

RuneSchema is a UE4SS runtime for self-contained RuneScape: Dragonwilds
content mods. It loads validated JSON/JSONC definitions, connects them to
mounted cooked assets, and provides multiplayer presentation and authority
bridges without replacing vanilla game systems.

## Documentation

- [AUTHORING-GUIDE.md](AUTHORING-GUIDE.md) — installation, mod layout, ordering,
  cooked assets, plugins, mappings, multiplayer, logs, and testing.
- [LOADER-WALKTHROUGHS.md](LOADER-WALKTHROUGHS.md) — walkthroughs for all 22
  loader folders with JSON examples and cross-loader references.
- [CHARACTER-CREATION-AUTHORING.md](CHARACTER-CREATION-AUTHORING.md) — all
  verified character-menu categories, tables, fields, and replay behavior.
- [COMPATIBILITY-BACKBONE.md](COMPATIBILITY-BACKBONE.md) — storefront, USMAP,
  and plugin compatibility behavior.
- [API-REFERENCE.md](API-REFERENCE.md) — complete plugin ABI, host functions,
  lifecycle, core services, mapping queries, and example plugin.
- [REGISTRY-PATCHING.md](REGISTRY-PATCHING.md) — transactional `/raw` registry
  patches.
- [BUILDING-CLONING-FMODEL-AUDIT.md](BUILDING-CLONING-FMODEL-AUDIT.md) and
  [raw/BASE-BUILDER-IMPORT.md](raw/BASE-BUILDER-IMPORT.md) — building cloning
  and imported assemblies.

Start with the authoring guide. The older audit files record implementation
decisions and are not authoring specifications.

Run `..\build\build.bat -Clean` to build the release:

- One universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime.
- Steam/GOG uses UE4SS's native object/reflection APIs and validated native signatures.
- Game Pass uses its matching UE4SS runtime and `UE4SS_Signatures` overrides where supplied.

Helpy is built once as a storefront-neutral RuneSchema API client. The output
is `dist\RuneSchema-0.7.5.22-Universal.zip`. A plugin-free
`RuneSchema-0.7.5.22-Core.zip` is emitted as proof that plug-ins are optional;
the two verified UE4SS runtime ZIPs are copied beside both. `-Clean` recreates
build and distribution directories.

Release archives contain an empty `RuneSchema\mods` directory and do not ship
`mods\runeschema.txt`. Extract updates over an existing RuneSchema directory;
do not delete that directory first. Existing mod folders, load order, settings,
and the small previous-run ownership snapshot are user data and must be retained.
The snapshot contains identities and owners only. Each successful startup
replaces it with the current active set; it never stores or restores character
items, quantities, progress, or other save state.

An optional current `.usmap` can be placed under `RuneSchema\dlls\mappings`, the
UE4SS root, or `ue4ss\mappings`. RuneSchema fingerprints it once at startup and
parses it only after an explicit mapping-service query. Fingerprint differences
are diagnostic; mutations are always validated against live reflection.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
