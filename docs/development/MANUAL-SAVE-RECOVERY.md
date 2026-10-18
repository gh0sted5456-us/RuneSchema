# Manual save recovery

Use this procedure only when RuneSchema's automatic owned-content or appearance
cleanup cannot load the character far enough to repair it. Manual editing is a
last-resort recovery path, not the normal uninstall workflow.

## Rejected/corrupt character entry

`persistence.allowCorruptCharacterEntry` is an emergency compatibility switch,
disabled by default. In RuneSchema settings, open **General > Runtime >
Progress persistence** and use **Bypass rejected character loads (restart
required)**. Restart is required both to enable
and to disable the bypass.

When enabled, RuneSchema verifies the native character validator and
`ProcessPlayerStateLoad`, lets both execute normally, then accepts rejected
Boolean results at their verified return tails. This covers their callers
without manually calling either function through an assumed native signature.
If either return gate cannot be verified, neither hook is activated. Other
RuneSchema loaders and cleanup remain independent. The same setting is also
available in Helpy's Settings tab under persistence.

This is **not a save repair** and does not guarantee successful loading. Broken
JSON, missing required data, native deserialization failures and unrelated
crashes are not made safe by accepting validation. Subsequent automatic or
manual game saves can persist incomplete state. Back up the complete character
storage first, retain the original, and turn the bypass off after recovery.

Disable the standalone `CorruptCharacterBypass` before enabling this option.
RuneSchema refuses duplicate hooks when that mod is already loaded or enabled
in the normal UE4SS Mods directory. An `enabled.txt` marker still enables the
standalone mod even when its `mods.txt` entry is `0`.

## Safety rules

1. Close RuneScape: Dragonwilds completely. Do not edit a save while the game,
   a dedicated server, or a save-conversion tool has it open.
2. Copy the complete save folder to a separate backup directory. Do not keep
   the only backup beside the live file under the same name.
3. Preserve RuneSchema's `*.runeschema-before-clean*.bak` files. They are
   byte-for-byte pre-cleanup copies and may be newer than the game's `.backup`.
4. Repair only identities known to belong to the removed mod. Never delete an
   unknown persistence ID merely because it is unfamiliar.
5. Validate the edited JSON before replacing a live character save.
6. Keep the original backup until the repaired character has loaded, saved,
   exited cleanly, and synchronized successfully.

Character saves contain account and gameplay information. Do not attach them
to public bug reports. Redact player names, GUIDs, inventory, and world data.

## Steam/Epic character saves

The loose character saves are normally under:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\SaveCharacters
```

The filename is not authoritative. Open a copy and confirm
`meta_data.char_name` before changing it.

### Restore the complete character

This is the safest manual option when a known-good backup is recent enough:

1. Copy the entire `SaveCharacters` directory somewhere safe.
2. Choose the newest known-good `.backup`, `.runeschema_backup`, or
   `.runeschema-before-clean*.bak` for that character.
3. Copy that backup to a temporary `.json` filename.
4. Validate the temporary JSON.
5. Replace the live character `.json` with the validated copy.
6. Start the game, load the character, save once, exit normally, and retain the
   backup until cloud synchronization completes.

Restoring a complete backup also rolls back all progress made after that copy.
Use field-only appearance repair when the rest of the character is healthy.

### Repair only a missing appearance field

Appearance is stored at:

```text
Customization.CustomizationData
```

The native save keys are `BodyType`, `Head`, `HairPreset`,
`FacialHairPreset`, `SkinTone`, `HairColor`, `EyeColor`, and
`EyebrowColor`. Each field contains exactly a `dataTable` path and `rowName`.

Copy the complete affected field from a known-good backup or another character
with the same intended vanilla selection. Do not copy the whole
`CustomizationData` object unless every appearance choice should be replaced.

For example, the native no-hair selection is:

```json
"HairPreset": {
  "dataTable": "/Script/Engine.DataTable'/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets'",
  "rowName": "Preset_None"
}
```

Do not guess a beard, face, body, or color row. Those choices can be body-type
or preset specific. Copy the exact field object from a working vanilla save.

RuneSchema repairs unresolved appearance handles from a male/A safety profile
baked into the DLL. No character snapshot, external `Default.json`, or periodic
in-world appearance callback is used. Repair occurs only at the once-per-process
character-load preflight after all applicable live registries are ready.

### Validate the edited file

In PowerShell, validate a temporary copy without rewriting its formatting:

```powershell
Get-Content -LiteralPath 'C:\path\Character.repaired.json' -Raw |
  ConvertFrom-Json -ErrorAction Stop | Out-Null
```

No output means the JSON parsed successfully. Parsing success does not prove
that Unreal object paths or row names exist, so compare every replacement with
a known-good vanilla save before installation.

## Game Pass / WinGDK character saves

Game Pass uses Xbox Game Save containers under:

```text
%LOCALAPPDATA%\Packages\JagexLimited.Dominion_srxstwq7wczqa\SystemAppData\wgs
```

Do **not** open a GUID-named WGS payload, `container.N`, or `containers.index`
in a text editor and replace it in place. Those files form one provider-managed
container, and a direct edit can invalidate the index or be overwritten by
cloud synchronization.

The safe manual route is:

1. Close Dragonwilds.
2. Copy the complete WGS profile directory to a separate location.
3. Extract the profile with
   [XblContainerReader](https://github.com/LukeFZ/XblContainerReader), or use
   another container-aware tool. A Dragonwilds `Qjson` character entry has a
   `Data` payload containing ordinary UTF-8 character JSON.
4. Identify the character by `meta_data.char_name` and `meta_data.char_guid`.
5. Repair and validate a copy of that JSON using the same appearance structure
   described for Steam/Epic.
6. Import the repaired character through
   [RSDW Save Converter](https://github.com/RSDWArchive/RSDWSaveConverter).
   It requires an existing Game Pass character destination of the same type and
   creates a complete WGS backup before replacing it.
7. Launch Dragonwilds. If Xbox reports a synchronization conflict, select the
   repaired local copy, verify the character, save once, and exit normally.
8. Keep both the manual profile copy and the converter backup until the save has
   synchronized and loaded successfully on a later launch.

RSDW Save Converter stores its automatic WGS backups under:

```text
%LOCALAPPDATA%\RSDWSaveConverter\Backups
```

`XblContainerReader update` is an expert recovery path. Prefer the
Dragonwilds-specific converter because it performs a full backup and verifies
the installed payload. Never run either update path while the game is open.

## Owned items and recipes

Manual inventory cleanup is more dangerous than appearance repair because
inventory, loadout, pickup history, and recipe arrays refer to the same
persistence identities in several places. Prefer restoring a complete backup
or letting RuneSchema retry its ownership-ledger cleanup.

If expert repair is unavoidable, use the mod's declared persistence IDs and
remove only exact matches from the appropriate collections. A removed inventory
slot must also be removed from any `GameProgress.Loadout` entry that points to
its `PlayerInventoryItemIndex`. Recipe identities can appear in both
`GameProgress.Progress.RecipesUnlocked` and `RecipesNew`. Preserve unrelated
entries and `MaxSlotIndex`; do not renumber inventory slots.

After any manual repair, keep the mod disabled, start RuneSchema, and inspect
`UE4SS.log` for `[PERSISTENCE-PRUNER]` or appearance-fallback errors before continuing
normal play.
