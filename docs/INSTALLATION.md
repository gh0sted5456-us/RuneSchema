# Install or update RuneSchema

This guide covers a normal RuneSchema installation and the safest way to update
an existing setup. The same RuneSchema content format is used on Steam, Epic,
and Game Pass. Use the package intended for your game and UE4SS installation.

New to the framework? Start with [what RuneSchema is, what its files do, and
how to make an add-on](MOD-HOSTS-AND-GETTING-STARTED.md).

## Before installation

You need:

- a working Dragonwilds installation;
- the supported UE4SS setup for your store version;
- the RuneSchema release package;
- a backup of any character used for testing.

Close the game before replacing RuneSchema, UE4SS, or any PAK files.

Choose the [0.8.0.2 release package](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.2/RuneSchema-0.8.0.2.zip)
and read its [defaults and verification](RELEASE-0.8.0.2.md).
There is one `RuneSchema-<version>.zip` per release, with the registry and
networking bridges and Helpy included. No separate Core or Universal choice is
needed. Older `e`/`m` suffixes are historical. The published 0.8.0.2 download
includes the earlier 0.8.0.1b source-only fixes. Check
[downloads and source updates](CURRENT-RELEASE.md) before choosing.

The cleaned runtime ZIP contains no generated mapping or loose UI artwork.
Mappings build/reuse the platform AppData cache; optional pictures can be
downloaded from Helpy. Preserve existing settings when updating. The supplied
defaults enable automatic Helpy opening and detailed logging; both are adjustable.

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

If a mod uses the optional `logicmods` folder, RuneSchema's own Lua helper can
start its cooked ModActor. The release settings enable this route without
editing BPModLoaderMod. Keep your existing `settings` and `mods` folders when
updating, and confirm `luaActorLoader = true` and
`patchScript = false` in the `[bpModLoader]` section of `settings.ini`. Existing
JSONC settings migrate automatically; see [settings and migration](SETTINGS-INI.md). A mod's
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

Choices are retained for temporarily missing folders, so restoring an update
does not reactivate a disabled mod. Only `1` enables a folder; invalid values
remain disabled. Save this file as UTF-8. If it changes while the load-order
editor is open, reload from disk before saving; stale edits are not applied.

Helpy opens as a separate desktop window with F2. It has its own taskbar entry
and normal keyboard/mouse focus. Press F2 again or close the window to return
to the game. Switching to another application does not force it closed.

## Check for updates in Helpy

Open **Settings > Updates & runtime** to compare your installed RuneSchema
version with GitHub main and the latest published stable release. Automatic
checks run in the background when Helpy opens, at most once a day. The same
panel has a manual check and links to GitHub and the community Nexus listing.
Disable automatic checks there if you prefer an offline setup.

Only public version metadata is requested. Helpy sends no character or world
data, downloads no update files and never installs an update automatically.
Results are cached under `%LOCALAPPDATA%\RSDragonwilds\RuneSchema\core\HELPY\updates.json`.
Network failures and rate limits appear as unavailable checks, not as proof that
your installation is current. An alpha version newer than main is shown as ahead;
different legacy build labels are not treated as ordered version numbers.

Nexus comparison opens its listing in your browser. Helpy does not bypass Nexus
login or anti-bot protections; its automatic checks use GitHub.

## UE4SS build identity and compatibility

Helpy shows the loaded UE4SS build in its runtime footer and Updates & runtime
panel. The current startup banner supplies the declared version and Git commit;
RuneSchema verifies the commit exists in the loaded DLL before reporting it.
If that identity cannot be established, the build remains unknown rather than
being guessed from a download filename or another installation's log.

RuneSchema has no runtime build-number ceiling or exact-version rejection rule.
It detects available capabilities and uses fallbacks for optional APIs. Its
compile SDK remains pinned for reproducible builds; that is not an installed-host
version lock. Newer UE4SS hosts must retain RuneSchema's required exported C++
symbols and ABI. A future breaking ABI change can require a rebuilt RuneSchema,
so compatibility with every future UE4SS binary cannot be guaranteed. RuneSchema
updates do not replace or modify `UE4SS.dll`.

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

For the current published ZIP, both RuneSchema and Helpy should report `0.8.0.2`.
Compare against the version of the package you installed.
If the versions are mixed or unexpectedly old, check
for an old DLL in the active UE4SS installation before troubleshooting the mod.

## Remove RuneSchema temporarily

Set `RuneSchema : 0` in the UE4SS `Mods/mods.txt` file and restart the game.
RuneSchema should not load at all.

Disabling RuneSchema also disables its save cleanup. If a character contains
references to removed mod content, restore that content or re-enable RuneSchema
before trying to load the character.
