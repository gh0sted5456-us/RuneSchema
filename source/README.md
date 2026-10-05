# RuneSchema

RuneSchema is a UE4SS runtime for **RuneScape: Dragonwilds** content mods.

It loads JSON/JSONC definitions, connects them to cooked assets, and provides
the runtime and multiplayer services used by the loader system.

## Documentation

For mod authors:

- [Authoring Guide](AUTHORING-GUIDE.md)
- [Loader Reference](LOADER-WALKTHROUGHS.md)
- [Example Library](EXAMPLES.md)
- [Ask RuneSchema](ASK-RUNESCHEMA.md)
- [Character Creation](CHARACTER-CREATION-AUTHORING.md)
- [Registry & DataTables](REGISTRY-PATCHING.md)
- [Building Cloning](BUILDING-CLONING.md)
- [Compatibility](COMPATIBILITY-BACKBONE.md)
- [Save Cleanup & Ownership](SAFE-SAVE-AND-LEDGER.md)
- [Manual Save Recovery](MANUAL-SAVE-RECOVERY.md)
- [Unreal + RuneSchema](unreal-runeschema/index.md)

For RuneSchema contributors and plugin authors:

- [Developer Guide](DEVELOPER-GUIDE.md)
- [Plugin API](API-REFERENCE.md)
- [Current Release](CURRENT-RELEASE.md)

## Updating an install

Extract the current RuneSchema archive over the existing RuneSchema directory.
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

The public documentation describes the current supported release. Historical
development checkpoints remain in Git history instead of the public guide.

## Community and license

RuneSchema is developed and maintained by the **RSDW Modding Community**.
Credited members: **Jonesing4Space, NuLLZz, Snorkles, and CHP**.
Published under **gh0sted5456-us**. See [community credits](../AUTHORS.md).

Copyright (c) 2026 RSDW Modding Community. Original software and documentation
owned by, or licensed with authority by, the community are available under the
[MIT License](../LICENSE). See [license scope](../LICENSING.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md) for component-specific terms.

PalSchema foundation by Okaetsu. UE4SS and other dependencies retain their
original copyright and license notices.
