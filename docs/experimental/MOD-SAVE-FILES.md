---
channel: experimental
promote_to: MOD-SAVE-FILES.md
---

!!! info "Experimental · 0.8.1.0"
    This describes an unpublished candidate. [Experimental overview](index.md).

# Mod-owned save files

Mods keep their own save formats and write their own files. A declaration lets
Helpy show those files under **Mods → right-click a mod → Mod saves…** and create
a verified manual backup in that installation's RuneSchema AppData folder.
The files are copied as opaque bytes; this does not add a shared save format or
move mod data out of the game's existing saves.

## Declare one save in ID.txt

```ini
SaveFileName=JBBB_save
SaveFileExtension=.sav
SaveFileRoot=GameSaved
SaveFilePath=SaveGames
```

This resolves to `RSDragonwilds/Saved/SaveGames/JBBB_save.sav` below Local AppData.
Use a literal basename and separate extension. `SaveFilePath` is optional and
relative to the selected root. These fields may sit in a `[SaveFile]` section;
fields inside `[Tips]` are ignored.

## Declare several files in cache/savefiles.json

```json
{
  "SchemaVersion": 1,
  "Files": [
    {"Name": "JBBB_save", "Extension": ".sav", "Root": "GameSaved", "Path": "SaveGames"}
  ]
}
```

Helpy merges this list with ID.txt, deduplicating identical declarations.
Cache rebuilds preserve this sidecar, including mod creation packages.

| Root | Location |
| --- | --- |
| `GameSaved` | Dragonwilds' Saved directory; the server's installation-owned Saved directory for dedicated servers |
| `LocalAppData` | Current user's Local AppData |
| `RoamingAppData` | Current user's Roaming AppData |
| `Mod` | This installed RuneSchema mod's root |

Paths must remain below their named root. Absolute paths, wildcards, traversal,
Windows device names and redirected paths are rejected. The declaration is a
backup contract, not permission to execute code or modify the source file.

## Back up

Close Dragonwilds and any other program writing these files. Review the resolved
locations, then choose **Back up declared saves**. Missing files are annotated;
existing files are locked against writers, copied, and SHA-256 verified. A backup
is complete only when its manifest is committed. Changed declarations or a game
restart cancel the operation and retain all source files.

Backups live below `mod-save-backups` in the current runtime's RuneSchema AppData
data directory. Each snapshot records its mod/version, original locations, file
sizes and hashes. **Open backup folder** shows the completed snapshot.
Automatic snapshots and restore actions for these custom files are not part of
this first version. Character/world recovery keeps its existing controls.
