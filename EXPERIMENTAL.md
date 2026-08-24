# RuneSchema 0.6.3 Experimental

This branch is the community testing line for changes that have not been merged into the official RuneSchema release. It is based on the upstream `main` branch and identifies itself at runtime as `0.6.3 Experimental`.

## Highlights

- An expanded UE4SS RuneSchema control panel with Overview, Settings, Generators, Load Order, and Compatibility pages.
- A manually editable `RuneSchema/mods/mods.txt` with deterministic ordering and `1`/`0` enable states.
- Load-order checkboxes and Up/Down controls that save back to `mods.txt`.
- Optional compatibility reports, expanded JSON schemas, and FModel snippet generation.
- Centralized normalization for FModel numeric export suffixes such as `.0`.
- `Scale` support for persistent Actor spawns and AI spawned by `AISpawnPoint` entries.
- Optional, independent `DropIncreasePercent` support for known live-instance `ItemsToDrop` layouts.
- Optional, registry-aware `PersistenceID` and `InternalName` overrides for asset, recipe, and journal files.

## Optional identity overrides

`PersistenceID` and `InternalName` can be supplied at the top level of supported asset, recipe, and journal entries. Omitting them—or setting them to `null`, `""`, or whitespace—preserves the existing/default identity. Recipe identity fields may also be placed inside `Properties`, although top-level values take precedence.

Identity values must be unique. RuneSchema rejects collisions and synchronizes the live persistence/internal-name lookup maps when an override is accepted. The feature performs work only while mod files are loading; it adds no polling or per-frame tick.

The loader directory remains singular: journal entries belong under `RuneSchema/mods/<mod name>/journal/`.

Asset entry:

```json
{
  "/Game/Mods/Example/DA_Example.DA_Example": {
    "PersistenceID": "example-item-v1",
    "InternalName": "example_item"
  }
}
```

New recipe or journal entry:

```json
{
  "ExampleEntry": {
    "PersistenceID": "example-entry-v1",
    "InternalName": "example_entry",
    "Properties": {}
  }
}
```

For journals, omit `Properties` and place the normal journal fields beside the two identity fields.

## Independent size and drop controls

`Scale` changes size only. Drops remain unchanged unless `DropIncreasePercent` is explicitly present and `enableExperimentalDropScaling` is enabled in `config/config.json`.

```json
{
  "Type": "Actor",
  "Id": "ExampleResource",
  "Class": "/Game/Example/BP_Resource.BP_Resource_C",
  "Location": { "X": 0, "Y": 0, "Z": 0 },
  "Scale": 2.0,
  "DropIncreasePercent": 50
}
```

Here, the actor is twice normal size and supported drop counts are multiplied by `1.5`. Drop scaling is experimental and only affects `ItemsToDrop` on the actor or the known item-drop, split-drop, and destruction-drop components.

## Distribution

Compiled builds are published as prereleases in this fork's GitHub Releases section. Build output and local RuneSchema configuration are intentionally excluded from Git history.

Installation, flavor selection, and compatibility details are maintained in the [Dragonwilds Sync modder documentation](https://gh0sted5456-us.github.io/Dragonwilds-Sync-Web/for-modders.html?build=b4a5199#runeschema-flavors).

This is a community experimental variant based on RuneSchema. Questions and experimental-build bug reports belong in the [Dragonwilds Sync issue tracker](https://github.com/gh0sted5456-us/Dragonwilds-Sync/issues), not the official RuneSchema support channels.

## Contributing upstream

Keep experimental work on this branch or a branch based on it. Stable, reviewable pieces can be proposed to the official repository through focused pull requests.
