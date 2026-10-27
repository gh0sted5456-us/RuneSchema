# Blueprint and Lua companion folders

Keep a mod’s PAKs, Blueprint companions and Lua packages together in its own
folder. Pick the route below that matches the content you already have.

## Step-by-step workflow

1. Choose ordinary cooked content (`paks`), a client ModActor (`logicmods`) or a
   supported Lua-only companion (`ue4ss`). Normal item discovery needs no startup actor.
2. Follow the matching folder layout below inside the owning enabled mod; keep
   package names and paths consistent with the actual cooked package/add-on.
3. Enable only the required helper route. Connect gameplay data through its
   ordinary JSONC loaders; a companion does not replace native registration.
4. Restart and verify startup, world changes, reconnect and cleanup on the intended
   roles. Prevent duplicate actors and callbacks.

These directory layouts are mock-ups. Lua/Blueprint startup is not a JSONC
directive: use [assets](reference/loaders/assets.md),
[registry](reference/loaders/registry.md), [quests](reference/loaders/quests.md)
and their JSONC mock-ups for the content those companions consume.

RuneSchema has two optional folders for behavior that an ordinary cooked PAK or
JSON file cannot start by itself. They look like RuneSchema loaders, but pass
the work to UE4SS. Keep them inside the mod they belong to so one
`runeschema.txt` line controls the whole mod.

| Folder inside `RuneSchema/mods/MyMod/` | What it does |
| --- | --- |
| `paks` | Makes cooked items, sounds, meshes, and other content available. It does not start a Blueprint actor. |
| `logicmods` | Mounts a cooked package and asks RuneSchema's Lua helper to start its `ModActor` on clients. |
| `ue4ss` | Complete `ue4ss/<PackageName>/` Lua-only add-ons using stock UE4SS; native/hybrid packages are skipped. |

Most mods need only `paks` and RuneSchema's normal content folders. Add the
other two only when the mod actually uses them.

## Start a cooked Blueprint LogicMod

Put one complete cooked package in its own folder:

```text
ue4ss/Mods/RuneSchema/mods/MyMod/
  logicmods/
    MyBlueprintPackage/
      MyBlueprintPackage.pak
      MyBlueprintPackage.utoc
      MyBlueprintPackage.ucas
```

The package needs a cooked `ModActor` at
`/Game/Mods/MyBlueprintPackage/ModActor.ModActor_C`. The *folder* names the
cooked package; the three container filenames may include version suffixes,
but must match one another. RuneSchema mounts the package and gives its name
to RuneSchema's Lua helper. It does not start every PAK it finds under `paks`.

If an older version of your mod kept this package under `paks`, move the
entire package folder to `logicmods`; do not leave a second copy behind.
Do not install the same package again under the game's
`Content/Paks/LogicMods` folder.

RuneSchema prepares a short enabled-package list when the game starts. With
`bpModLoader.luaActorLoader: true` and `patchScript: false` in
`RuneSchema/settings/settings.ini` (legacy JSONC is supported), its own helper starts those ModActors
without changing BPModLoaderMod. Restart the game after changing the mod list.
The log gives a startup summary; the exact package names are in
`RuneSchema/settings/logicmods.lua.generated.txt`. Existing installations
can keep their settings file and enable this one option manually.

This actor-startup path is for clients, including a player hosting co-op.
Dedicated servers still receive the cooked package but do not start a visual
ModActor. Blueprint startup alone does not make custom behavior networked;
the mod must use game-supported multiplayer behavior.

## Include UE4SS packages

Place Lua files inside the same RuneSchema mod:

```text
ue4ss/Mods/RuneSchema/mods/MyMod/
  ue4ss/
    MyLuaPackage/
      Scripts/main.lua
      Scripts/support.lua
      config.txt
    AnotherLuaPackage/
      Scripts/main.lua
```

Packages start once per launch, not on world changes. Preserve their entire
contents. Only Lua-based add-ons are supported; UE4SS.dll is not replaced or
patched. Native/hybrid packages are skipped. The old direct
`ue4ss/scripts/main.lua` layout remains compatible. See the
[package guide](UE4SS-LUA-COMPANIONS.md) for requirements and safety.

## Turn the whole mod off

Use `MyMod : 0` in `RuneSchema/mods/runeschema.txt`. RuneSchema then excludes
that mod's PAKs, LogicMods, and Lua companion on the next launch. Turning
RuneSchema itself off in UE4SS `Mods/mods.txt` also prevents its nested
companions from starting. A separately installed copy outside RuneSchema has
its own enable state and must be managed separately.

If the LogicMod summary says no packages were queued, confirm the package is
under `logicmods/<PackageName>/`, all three cooked files are present, RuneSchema
is enabled, and you restarted the game. See the
[PAK guide](PAK-CONTENT-GUIDE.md#start-a-cooked-blueprint-modactor) for the
full cooked-content layout.
