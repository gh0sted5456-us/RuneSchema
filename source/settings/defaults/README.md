# Appearance default override

RuneSchema contains a built-in male/A appearance profile. No external file is
required for normal recovery.

To replace the built-in recovery appearance, copy `Default.example.jsonc` to
`Default.json` in this directory, remove comments if you add any, edit the
eight canonical fields, and enable `defaults.appearanceOverrideEnabled`.
Restart the game after changing it.

The active path is:

```text
UE4SS\Mods\RuneSchema\settings\defaults\Default.json
```

Every `dataTable` and `rowName` must resolve in the live game after all PAKs and
RuneSchema loaders finish registering. The override must contain exactly these
fields: `BodyType`, `Head`, `HairPreset`, `FacialHairPreset`, `SkinTone`,
`HairColor`, `EyeColor`, and `EyebrowColor`.

If the file is missing, malformed, incomplete, too large, or references a row
that is not live, RuneSchema ignores it and uses its built-in profile. The
override never controls persistence-ID pruning.

## Restore selected save sections

`defaults.restoration.enabled` is a separate, disabled-by-default safe baseline
merge. For this mode, `Default.json` must be a complete gameplay character JSON
containing `GameProgress`. You can copy a known-good loose character save here
and rename it `Default.json`. Keep the original backup elsewhere.

Enable only the restoration categories you intend to repair: appearance,
inventory, personal inventory, loadout, item/recipe progress, quests,
journal/lore, or remaining `GameProgress` fields. In safe merge mode, valid live
state wins: missing keyed entries are added, set-like progress is unioned, and
existing scalar progress is retained. The baseline never deletes healthy live
progress.

`defaults.restoration.resetOnce` is the separate destructive option. When set,
the selected categories are replaced from the snapshot for the first eligible
character loaded on the next RuneSchema launch. RuneSchema writes
`resetOnce: false` back to settings before character
loading; if that write fails, the reset is refused. Selecting every category
for a one-shot reset restores the complete gameplay snapshot while still
retaining the active character's `meta_data`, including its GUID and name.

Restoration occurs at the once-per-process startup character boundary.
RuneSchema builds the merge or reset in memory, then runs mandatory
live-registry pruning over the result,
and finally performs one reflected writeback. If the snapshot or any requested
section is missing, no restoration category is applied and normal pruning
continues.
