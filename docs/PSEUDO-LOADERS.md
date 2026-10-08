# Blueprint and Lua companion folders

RuneSchema has two optional folders for behavior that an ordinary cooked PAK or
JSON file cannot start by itself. They look like RuneSchema loaders, but pass
the work to UE4SS. Keep them inside the mod they belong to so one
`runeschema.txt` line controls the whole mod.

| Folder inside `RuneSchema/mods/MyMod/` | What it does |
| --- | --- |
| `paks` | Makes cooked items, sounds, meshes, and other content available. It does not start a Blueprint actor. |
| `logicmods` | Mounts a cooked package and asks BPModLoaderMod to start its `ModActor` on clients. |
| `ue4ss` | Starts an optional Lua companion from `ue4ss/scripts/main.lua`. |

Most mods need only `paks` and RuneSchema's normal content folders. Add the
other two only when the mod actually uses them.

## Start a cooked Blueprint LogicMod

BPModLoaderMod must be installed and enabled in UE4SS. Put one complete cooked
package in its own folder:

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
to BPModLoaderMod. It does not start every PAK it finds under `paks`.

If an older version of your mod kept this package under `paks`, move the
entire package folder to `logicmods`; do not leave a second copy behind.
Do not install the same package again under the game's
`Content/Paks/LogicMods` folder.

RuneSchema prepares a short enabled-package list when the game starts. It
updates a compatible BPModLoaderMod script safely and keeps a backup before
changing it. Restart the game after changing the mod list or updating either
program. The log gives a submission summary; the exact package names are in
`RuneSchema/settings/logicmods.generated.txt`.

This actor-startup path is for clients, including a player hosting co-op.
Dedicated servers still receive the cooked package but do not start a visual
ModActor. Blueprint startup alone does not make custom behavior networked;
the mod must use game-supported multiplayer behavior.

## Add an optional UE4SS Lua companion

Place Lua files inside the same RuneSchema mod:

```text
ue4ss/Mods/RuneSchema/mods/MyMod/
  ue4ss/
    scripts/
      main.lua
      support.lua
```

The entry file must be named `main.lua`. RuneSchema starts it once per game
launch, not every time a player changes worlds. It can use normal UE4SS Lua
functions. This folder currently carries Lua, not DLL mods. See the
[Lua companion guide](UE4SS-LUA-COMPANIONS.md) for a working example and
multiplayer guidance.

## Turn the whole mod off

Use `MyMod : 0` in `RuneSchema/mods/runeschema.txt`. RuneSchema then excludes
that mod's PAKs, LogicMods, and Lua companion on the next launch. Turning
RuneSchema itself off in UE4SS `Mods/mods.txt` also prevents its nested
companions from starting. A separately installed copy outside RuneSchema has
its own enable state and must be managed separately.

If the LogicMod summary says `submitted=0`, confirm the package is under
`logicmods/<PackageName>/`, all three cooked files are present, both RuneSchema
and BPModLoaderMod are enabled, and you restarted the game. See the
[PAK guide](PAK-CONTENT-GUIDE.md#start-a-cooked-blueprint-modactor) for the
full cooked-content layout.
