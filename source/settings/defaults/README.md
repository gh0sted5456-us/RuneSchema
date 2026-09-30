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

`defaults.restoration.enabled` is a separate, disabled-by-default recovery
feature. For this mode, `Default.json` must be a complete gameplay character
JSON containing `GameProgress`. You can copy a known-good loose character save
here and rename it `Default.json`. Keep the original backup elsewhere.

Enable only the restoration categories you intend to copy: appearance,
inventory, personal inventory, loadout, item/recipe progress, quests,
journal/lore, or remaining `GameProgress` fields. Selecting every category
restores the complete gameplay snapshot while retaining the active character's
`meta_data`, including its GUID and name.

Restoration occurs once at the startup character boundary. RuneSchema performs
the copy in memory, then runs mandatory live-registry pruning over the result,
and finally performs one reflected writeback. If the snapshot or any requested
section is missing, no restoration category is applied and normal pruning
continues.
