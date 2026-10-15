# Install RuneSchema on a dedicated server

Stop the dedicated server completely before changing its UE4SS installation,
RuneSchema DLL, PAKs, or mod list. Preserve the existing installation and
`RSDragonwilds/Saved/SaveGames` first.

## 1. Find the server's UE4SS folder

Use the folder next to the dedicated-server executable, normally:

```text
RSDragonwilds/
  Binaries/
    Win64/
      RSDragonwildsServer-Win64-Shipping.exe
      ue4ss/
        Mods/
```

Use the **server** folder, not the Steam client's `Win64` folder and not the
Game Pass client's `WinGDK` folder. If UE4SS is not installed for that server
build, set it up before copying RuneSchema.

## 2. Install the single package

Extract the [current RuneSchema ZIP](../CURRENT-RELEASE.md) so the server has:

```text
ue4ss/Mods/RuneSchema/
  dlls/main.dll
  mods/runeschema.txt
  plugins/plugins.txt
  scripts/main.lua
  settings/settings.ini           # legacy settings.jsonc is supported
```

Keep the package's bridge PAK triplets intact. Copy your server's existing
`mods` folder, `runeschema.txt`, and settings aside before an update, then
restore your chosen configuration without mixing DLLs from old releases.

In UE4SS `Mods/mods.txt`, enable RuneSchema itself:

```text
RuneSchema : 1
```

## 3. Install gameplay mods deliberately

Put each RuneSchema mod under `RuneSchema/mods/<ModName>`. List each enabled
folder in `RuneSchema/mods/runeschema.txt`, with a required base mod before a
dependent add-on:

```text
BaseContent : 1
ServerExpansion : 1
DisabledTrial : 0
```

Do not put a second copy of the same PAK in the game's `Content/Paks` folder.
`0` stops RuneSchema from loading the folder, but cannot stop another copy
installed outside it. Read the mod author's server/client requirements and
[align the clients](mod-parity.md).

## 4. Start and verify

Start the dedicated server and inspect **that server's** `UE4SS.log`:

1. Confirm RuneSchema's version and successful startup.
2. Confirm the enabled mod names and mounted PAK packages.
3. Confirm required bridge plugins loaded.
4. Check item, recipe, building, quest, and combat registration summaries
   relevant to your mod list.
5. Resolve the first failure before using an important world.

The RuneSchema authoring tab can be present on a server, but a headless
server does not render a player UI. Do not treat absence of a visible panel
as evidence that its loaders failed.

## 5. Join-test with a matching client

Try one client with the required gameplay PAKs. Verify item use, crafting,
world entry, leaving, and rejoining. A dedicated server's successful mount
does not install missing meshes, sounds, or widgets on the client.

If a LogicMod unexpectedly starts on the dedicated server, follow the
[LogicMod troubleshooting steps](../TROUBLESHOOTING.md#a-logicmod-starts-on-a-dedicated-server).
