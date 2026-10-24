# Manage mods and load order

RuneSchema uses two enable lists. The first starts RuneSchema. The second
controls the content folders RuneSchema reads.

## 1. Keep RuneSchema enabled in UE4SS

Open `ue4ss/Mods/mods.txt` and check for:

```text
RuneSchema : 1
```

`0` here turns off RuneSchema itself. Its loaders and save cleanup will not
run. Do not use this line to disable just one content mod.

## 2. Enable content in RuneSchema's list

Open `ue4ss/Mods/RuneSchema/mods/runeschema.txt`:

```text
BaseWeapons : 1
WeaponExpansion : 1
OldWorldTest : 0
```

Use the exact folder name on each line. `1` enables the mod; `0` disables it
for the next game launch. A disabled entry wins over a duplicate enabled entry
for the same name, even if the letter case differs. Remove duplicate lines to
make the intended order obvious.

Read the list from top to bottom. If `WeaponExpansion` needs `BaseWeapons`,
place the base first. RuneSchema does not silently sort mods alphabetically.

## 3. Know what a disabled folder stops

RuneSchema does not load that folder's JSON/JSONC definitions, PAKs, LogicMods,
or nested UE4SS Lua companion. It cannot stop a second copy of the same PAK
installed directly in the game's ordinary PAK folder or another enabled mod.
If a disabled feature remains visible, search for a duplicate installation.

Optional shipped plugins have a third list,
`RuneSchema/plugins/plugins.txt`. Leave a bridge enabled when an installed mod
requires it. [Choose bridges](../OPTIONAL-BRIDGES.md).

## 4. Confirm the next launch

Close Dragonwilds fully, edit the list, then restart. In `UE4SS.log`, check
RuneSchema's startup version, enabled mod names, mount results, and loader
summaries. A PAK mounting is only the first step: its cooked item or recipe
must also resolve and join the game system that uses it.

To remove a gameplay mod from an existing save, follow the
[save-removal checklist](../SAFE-SAVE-AND-LEDGER.md#recommended-removal-process)
before loading your main character or world.
