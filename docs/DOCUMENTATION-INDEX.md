# RuneSchema 0.7.5.28 documentation index

This index organizes the authoritative documentation for the universal
RuneSchema 0.7.5.28 runtime. The release uses one `main.dll` with isolated
Steam/GOG and Game Pass/WinGDK execution lanes.

## Start here

1. `README.md` — project and release overview.
2. `AUTHORING-GUIDE.md` — installation, mod structure, ordering, cooked assets,
   mappings, multiplayer, logging, and build workflow.
3. `LOADER-WALKTHROUGHS.md` — every loader, accepted shapes, examples, and
   cross-loader behavior.
4. `SAFE-SAVE-AND-LEDGER.md` — ownership ledger and safe removal behavior.
5. `COMPATIBILITY-BACKBONE.md` — universal runtime, storefront detection,
   signatures, mappings, plugins, and failure isolation.

## Focused references

- `API-REFERENCE.md` — native plugin ABI and RuneSchema services.
- `CHARACTER-CREATION-AUTHORING.md` — character editor DataAssets, tables,
  fields, and runtime refresh rules.
- `REGISTRY-PATCHING.md` — `/raw`, `/assets`, and transactional registry edits.
- `BUILDING-CLONING-FMODEL-AUDIT.md` — cloned pieces, build-menu placement,
  imported assemblies, collision, HISM, and persistence.
- `GAMEPASS-SAVE-SYSTEM.md` — WGS directory, index/container structure,
  character/world payload formats, and provider boundary.
- `NATIVE-HOOK-PARITY.md` — verified Steam and WinGDK native capabilities.
- `HELpy-REDESIGN-AUDIT.md` — minimal on-demand UI and catalog behavior.
- `BASELINE-SUBSYSTEM-AUDIT.md` — baseline subsystem and compatibility audit.

## Machine-readable references

- `schemas/` contains the current JSON schemas.
- `examples/` contains loader and integration examples. Examples are authoring
  references and are not installed into a public runtime.

## Historical notes

`RELEASE-*.md` files are chronological implementation records. They should not
override the current authoring guide, loader walkthroughs, SafeSave guide, or
API reference when behavior changed in a later release.
