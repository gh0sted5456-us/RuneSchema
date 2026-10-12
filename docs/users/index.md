# RuneSchema for players

You do not need to write JSON or cook a PAK to use RuneSchema. Install the
framework once, then place compatible mods in its `mods` folder and choose
which ones load.

## The shortest route to playing

1. [Download the current package](../CURRENT-RELEASE.md) for your game build.
2. [Install or update RuneSchema](../INSTALLATION.md) while Dragonwilds is closed.
3. Put each RuneSchema add-on in its own folder under `RuneSchema/mods`.
4. [Enable the folders in `runeschema.txt`](manage-mods.md), keeping any required
   base mod above its add-ons.
5. Start the game and check that the expected mods and PAKs appear in
   `UE4SS.log` before loading an important save.

Keep a backup of a character and world you care about. A mod's content may be
saved in either one; disabling a mod is not the same as undoing everything it
did to a save.

## What is in the download?

The single RuneSchema ZIP includes its DLL and the shipped Helpy, registry,
and networking plugins. There is no separate Core or Universal choice. A mod
may require one of the optional bridges; read its requirements before turning
one off. [See what the bridges do](../OPTIONAL-BRIDGES.md).

RuneSchema itself is enabled in UE4SS `Mods/mods.txt`. Individual RuneSchema
mods are enabled in `RuneSchema/mods/runeschema.txt`. These are different lists.

## Common player tasks

| Task | Guide |
| --- | --- |
| Change which mods load, or their order | [Manage mods](manage-mods.md) |
| Move between Steam, GOG, Game Pass, or co-op | [Platform and multiplayer](../COMPATIBILITY.md) |
| Remove a gameplay mod carefully | [Save cleanup and safe removal](../SAFE-SAVE-AND-LEDGER.md) |
| A PAK mounts but something is missing | [Troubleshooting](../TROUBLESHOOTING.md) |
| Understand the files on a mod host | [File guide for CurseForge and Nexus](../MOD-HOSTS-AND-GETTING-STARTED.md) |

!!! important
    RuneSchema can check saved identities against the live game registries. It
    cannot restore an item, quest, or world object already deleted from a save,
    and a PAK that failed to mount must be fixed before cleanup can judge its
    content accurately.
