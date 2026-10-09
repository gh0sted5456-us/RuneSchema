# Install or update RuneSchema

This guide covers a normal RuneSchema installation and the safest way to update
an existing setup. The same RuneSchema content format is used on Steam, GOG,
and Game Pass. Use the package intended for your game and UE4SS installation.

## Before installation

You need:

- a working Dragonwilds installation;
- the supported UE4SS setup for your store version;
- the RuneSchema release package;
- a backup of any character used for testing.

Close the game before replacing RuneSchema, UE4SS, or any PAK files.

For releases after 0.7.7.5m, download the single `RuneSchema-<version>.zip`
package. It includes the registry and networking bridges needed by RuneSchema
mods, plus the Helpy plugin. There is no separate plugin-free release package.
The existing 0.7.7.5m release still has two legacy ZIPs; choose **Universal**
for that release.

## RuneSchema folder location

RuneSchema belongs inside the UE4SS `Mods` folder:

```text
ue4ss/
  Mods/
    RuneSchema/
      dlls/
      mods/
      scripts/
      settings/
```

The exact folders above `ue4ss` depend on the store installation. Do not move
the contents of `RuneSchema/mods` into the game's ordinary PAK folder.

The package also has a `RuneSchema/plugins` folder. Its
`plugins.txt` file controls the optional components shipped with RuneSchema:

```text
RuneSchema.RegistryBridge : 1
RSNetworking : 1
RuneSchema.Helpy : 1
```

Use `0` to keep one optional plugin from loading. Do not delete only one file
from a PAK triplet; disable the plugin or remove its whole folder instead.

## Enable RuneSchema itself

The UE4SS `Mods/mods.txt` file controls whether RuneSchema starts. Its line
must be enabled:

```text
RuneSchema : 1
```

Use `0` only when you want RuneSchema itself to stay completely off.

If a mod uses the optional `logicmods` folder, install and enable UE4SS
BPModLoaderMod as well. RuneSchema prepares its enabled package list and
updates a compatible BPModLoaderMod script at game launch. A mod's
`ue4ss/scripts/main.lua` companion needs no separate UE4SS `mods.txt` line.
See [Blueprint and Lua companion folders](PSEUDO-LOADERS.md) for the two layouts.

## Enable individual RuneSchema mods

RuneSchema has its own list at:

```text
RuneSchema/mods/runeschema.txt
```

Each folder has one line:

```text
MyWeaponPack : 1
MyStationPack : 1
OldTestMod : 0
```

`1` enables the folder. `0` disables it. A disabled line always wins if the
same name appears more than once, even with different letter case.

The line order is also the RuneSchema mod order. Keep required base mods above
the patches that depend on them.

## Update an existing installation

1. Close Dragonwilds.
2. Copy your current `RuneSchema` folder somewhere safe.
3. Keep your own mod folders and `runeschema.txt`.
4. Replace the RuneSchema program files with the new release.
5. Do not mix program files from two RuneSchema versions.
6. Start the game and wait at the main menu while the first setup finishes.
7. Read `UE4SS.log` for the RuneSchema startup summary.
8. Test world entry, menu return, and world re-entry.

If you previously installed RuneSchema, keep your preferred values
from `RuneSchema/plugins/plugins.txt` when updating. See
[Optional bridges](OPTIONAL-BRIDGES.md) before disabling a bridge required by a
content mod.

## Confirm a successful start

The log should show that RuneSchema started and reached its content-loading
steps. Some optional features may report that they are unavailable. That is not
the same as a failed start.

If RuneSchema cannot start at all, the log should state the reason. Fix that
first before judging whether an individual mod worked.

## Remove RuneSchema temporarily

Set `RuneSchema : 0` in the UE4SS `Mods/mods.txt` file and restart the game.
RuneSchema should not load at all.

Disabling RuneSchema also disables its save cleanup. If a character contains
references to removed mod content, restore that content or re-enable RuneSchema
before trying to load the character.
