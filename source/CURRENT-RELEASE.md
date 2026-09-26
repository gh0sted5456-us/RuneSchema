# Current release

This page summarizes the current public RuneSchema release.

## Main changes

### Universal runtime

- One `main.dll` supports Steam/GOG and Game Pass/WinGDK.
- Storefront-specific native code stays in separate runtime lanes.
- Missing optional hooks disable only the feature that needs them.
- UE4SS metadata and Unreal reflection remain the fallback where supported.

### Loader and authoring system

- Loader folders support nested JSON/JSONC organization.
- `/assets` supports direct reflected DataAsset/class-default edits and
  compatible item cloning.
- `/raw` supports DataTable row creation and patching plus strict registry
  transactions.
- Exact cooked DataTable paths are supported where the authoring contract calls
  for them.
- Character-menu edits can use the direct target/field form without a repeated
  registry envelope.
- `$MergeWhere`, `$AppendUnique`, owned-row transactions, dependency ordering,
  and reflected field checks reduce repeated definitions and unsafe writes.

### Character creation

- Character options can be added or updated across native editor categories.
- Hair and facial-hair authoring use native DataTables and menu DataAssets.
- Retained character-menu edits are replayed when the game rebuilds relevant
  class defaults.
- Appearance persistence is separate from loader activation.

### Recipes, journals, vendors, and progression

- Recipe placement supports exact DataTable paths, categories, direct recipe
  arrays, vanilla merchants, and RuneSchema vendors.
- Recipe placement and recipe unlocking are separate.
- Journal/lore registration can remain active when save-backed unlock
  persistence is disabled.
- Vendor filtering keeps category identity stable while applying time,
  power-level, and quest gates.

### Buildings and Base Builder

- Building clones inherit catalogue placement when `AddTo` is omitted.
- Cooked replacement actors are validated before use.
- Requirements replace the full source cost.
- Base Builder imports support native pieces, static assemblies, and hybrid
  layouts.

### Multiplayer and registry

- `/registry` provides the server-authority/client-presentation bridge.
- Server-owned gameplay actions remain validated on the server.
- Client presentation uses installed cooked assets and replicated world state.
- Duplicate persistent identities are rejected.

### SafeSave and persistence

- Cleanup is limited to identities previously owned by RuneSchema.
- Vanilla and unknown third-party records are left alone.
- Steam/GOG uses backed-up loose character files where appropriate.
- Game Pass cleanup uses hydrated game state and the Xbox Game Save provider
  instead of treating WGS files as Steam JSON.
- Recipe, journal, and character-customization persistence can be disabled
  independently of their loaders.

### Helpy

- Helpy remains optional.
- Opening the UI does not trigger a full UObject scan.
- Search, filtering, pagination, lazy icon work, and cached catalogue data keep
  the normal browse path bounded.
- UI work remains isolated from core loader behavior.

### Performance and diagnostics

- Journal finalization uses indexed reference lookup instead of repeated full
  object scans.
- Current reference measurements are about **306 ms on Steam/GOG** and
  **410 ms on Game Pass** for the profiled mod set.
- Loader success output is quieter while warnings, failures, and summaries stay
  visible.
- Errors remain scoped so unrelated mods and loaders can continue where safe.

## Packages

The public release provides:

- Universal RuneSchema runtime;
- Core-only RuneSchema runtime;
- Steam/GOG UE4SS runtime;
- Game Pass/WinGDK UE4SS runtime.

Exact package names, hashes, and installation notes are available in the
repository [release directory](https://github.com/gh0sted5456-us/RuneSchema/tree/main/release).

## Documentation

For current behavior, use:

- [Authoring Guide](AUTHORING-GUIDE.md)
- [Loader Reference](LOADER-WALKTHROUGHS.md)
- [Character Creation](CHARACTER-CREATION-AUTHORING.md)
- [Registry & DataTables](REGISTRY-PATCHING.md)
- [Building Cloning](BUILDING-CLONING.md)
- [Compatibility](COMPATIBILITY-BACKBONE.md)
- [SafeSave & Ownership](SAFE-SAVE-AND-LEDGER.md)

Implementation details and build flow are kept in the
[Developer Guide](DEVELOPER-GUIDE.md).
