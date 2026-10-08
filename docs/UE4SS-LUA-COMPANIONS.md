# Optional UE4SS Lua companions

RuneSchema mods can include a small UE4SS Lua companion when JSON and cooked
PAK content are not enough. The companion stays inside the RuneSchema mod, so
players do not need to add another named mod to UE4SS's `mods.txt`.

This is an optional tool. Most content should continue to use cooked game
assets and RuneSchema's normal folders.

## Folder layout

Use the following exact layout:

```text
RuneSchema/
└─ mods/
   └─ MyMod/
      ├─ assets/
      ├─ paks/
      └─ ue4ss/
         └─ scripts/
            ├─ main.lua
            └─ support.lua
```

The folder is named `ue4ss`, not `lua`. The entry file must be named
`ue4ss/scripts/main.lua`. Additional Lua files may be placed beside it and
loaded with `require`.

RuneSchema currently loads Lua files only from this folder. DLL companions are
not loaded.

## Enable the companion

The mod must be enabled in `RuneSchema/mods/runeschema.txt`:

```text
MyMod: 1
```

`MyMod: 0` disables the entire RuneSchema mod, including its Lua companion.
Disabling RuneSchema itself in UE4SS also disables every nested companion.

Restart the game after adding, removing, enabling, or disabling a companion.
Changing worlds does not start it a second time.

## Minimal example

Create `RuneSchema/mods/MyMod/ue4ss/scripts/main.lua`:

```lua
local context = RuneSchemaUE4SS.Current

print(string.format(
    "[MyMod] Lua companion started from %s\n",
    context and context.Root or "an unknown folder"
))

local support = require("support")
support.start()
```

Then create `support.lua` in the same `scripts` folder:

```lua
local M = {}

function M.start()
    print("[MyMod] Support module ready.\n")
end

return M
```

While `main.lua` is starting, `RuneSchemaUE4SS.Current` provides:

| Value | Meaning |
| --- | --- |
| `Name` | RuneSchema mod folder name |
| `Root` | That mod's `ue4ss` folder |
| `Scripts` | That mod's `ue4ss/scripts` folder |

Normal UE4SS Lua functions remain available.

## What appears in the log

A successful companion reports:

```text
[RuneSchema][UE4SS] [MOD:MyMod][OK] Lua companion started.
```

RuneSchema also prints one summary:

```text
[RuneSchema][UE4SS] Companion summary: 1 started, 0 failed.
```

`0 started, 0 failed` means the bootstrap ran correctly but none of the enabled
mods contained `ue4ss/scripts/main.lua` at the expected location.

If one companion fails, RuneSchema reports that mod's name and error while
continuing to the next enabled companion.

## Multiplayer guidance

Loading a Lua companion does not automatically send its actions over the
network. Multiplayer features should use the game's existing replicated
systems or a documented RuneSchema bridge. Install the same required content
on the server and clients unless the mod author clearly marks one side as
optional.

For example, a server can tell clients that a replicated game action occurred,
but a local-only Lua callback does not become synchronized merely because it is
inside RuneSchema.

## Safety

Lua companions run with normal UE4SS Lua access. Only install companions from
authors you trust. Keep the script focused on the feature that cannot be
expressed through cooked content or RuneSchema JSON.

