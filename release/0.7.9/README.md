# RuneSchema 0.7.9

RuneSchema 0.7.9 uses one runtime for Steam/GOG and Game Pass/WinGDK.

## Current package state

The 0.7.9 source changed after the previous local Core and Universal packages
were built. Rebuild from the current branch before publishing release assets.
Record package sizes and SHA-256 hashes here only after that build succeeds.

## Notes

- Advanced logging controls detail only; it never gates RuneSchema startup or loaders.
- Auto reload controls the file watcher only; initial loading is independent.
- Non-JSON/JSONC files and unrelated mod-manager folders are ignored by mod loaders.
- Plugin package discovery ignores unrelated files that are not supported container content.
- Helpy is a DLL-only plugin and does not mount PAK content.
- The networking plugin is named `RSNetworking`; leftover `RuneSchema.Networking`
  folders from older installs are ignored when the renamed plugin is present.
- One RuneSchema content definition is used in standalone and multiplayer.
- Steam/GOG and Game Pass/WinGDK keep separate native binding lanes selected at runtime.
- Normal RuneSchema recipe unlocks may persist once the live RecipeData has a valid PersistenceID.
- Safe Clean uses the same filtered active-mod discovery as the loader system.
