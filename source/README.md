# RuneSchema 0.7.5.28

RuneSchema is a UE4SS runtime for **RuneScape: Dragonwilds** content mods.

It loads JSON/JSONC definitions, connects them to cooked assets, and provides
the runtime and multiplayer services used by the loader system.

## Documentation

For mod authors:

- [Authoring Guide](AUTHORING-GUIDE.md)
- [Loader Reference](LOADER-WALKTHROUGHS.md)
- [Example Library](EXAMPLES.md)
- [Character Creation](CHARACTER-CREATION-AUTHORING.md)
- [Registry Patching](REGISTRY-PATCHING.md)
- [Building Cloning](BUILDING-CLONING.md)
- [Compatibility](COMPATIBILITY-BACKBONE.md)
- [Manual Save Recovery](MANUAL-SAVE-RECOVERY.md)

For RuneSchema contributors and plugin authors:

- [Developer Guide](DEVELOPER-GUIDE.md)
- [Plugin API](API-REFERENCE.md)

## Updating an install

Extract the new RuneSchema archive over the existing RuneSchema directory.
Do not delete the directory first.

Keep your existing:

- mod folders;
- load order;
- settings;
- RuneSchema ownership state.

The ownership state records IDs and owners. It is not a character-save backup
and does not restore removed inventory or progress.

## Mappings

An optional current `.usmap` may be placed in:

- `RuneSchema\dlls\mappings`
- the UE4SS root
- `ue4ss\mappings`

RuneSchema fingerprints the selected map and parses it only when a mapping
query needs it. Runtime reflection still validates object and field writes.

## Build

Contributor build and packaging notes are in the
[Developer Guide](DEVELOPER-GUIDE.md).

## Release

The public documentation tracks the Nexus-published **0.7.5.28** release.
Intermediate 0.7.x development notes remain available in Git history instead of
separate website pages.

Based on the original RuneSchema 0.6.0 by Snorkles. PalSchema foundation by
Okaetsu. Maintained by members of the RSDW Modding Community.
