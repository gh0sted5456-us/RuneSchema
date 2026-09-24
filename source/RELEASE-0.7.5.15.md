# RuneSchema 0.7.5.15

- Every loader now discovers `.json` and `.jsonc` files throughout its complete nested directory tree in deterministic relative-path order.
- Nested folders are organizational only and do not change loader ownership or generated runtime identity.
- `/assets` supports explicit reflected `DA_` object and Blueprint class-default-object edits through `RuneSchema.AssetPatch.v2`.
- Safe nested operations include `Set`, `Merge`, `Append`, and `AppendUnique` across reflected structs, maps, arrays, and object references.
- Hair mods can append unique entries to `DA_CharacterOptionData_C` without replacing the cooked vanilla DataAsset.
- Full target paths, explicit CDO mode, expected-class checks, bounded discovery, and no raw offsets remain mandatory.
- The universal DLL now selects one explicit native-binding lane: `steam-native`, `gamepass-ue4ss`, or conservative `shared-safe`.
- Embedded Win64 AOB signatures are Steam/GOG-only. Game Pass preserves UE4SS's WinGDK bindings and uses shared reflection/vtable fallbacks instead of overwriting missing bindings with null.
- Field-class discovery uses UE4SS's populated local field-class registry and safely returns an unavailable capability when a class is absent; it no longer calls the old game signature or dereferences a missing map.

See `AUTHORING-GUIDE.md`, `LOADER-WALKTHROUGHS.md`, and `schemas/asset-patch-v2.schema.json`.
