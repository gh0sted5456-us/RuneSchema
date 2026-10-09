# Local LogicMod loader experiment

This worktree is a local-only test of building recovery and RuneSchema-owned
LogicMod startup. It is not a GitHub release and is not installed into the game.

In `RuneSchema/settings/settings.jsonc`, choose one startup owner:

- Default: `bpModLoader.luaActorLoader: false` and `patchScript: false` uses
  RuneSchema's native ModActor fallback. It leaves BPModLoaderMod untouched.
- Experimental: set `luaActorLoader: true` to let RuneSchema's own UE4SS Lua
  helper start RuneSchema-owned cooked ModActors. RuneSchema must be enabled in
  UE4SS `mods.txt`; disabled RuneSchema mods in `runeschema.txt` are omitted.
- Manual compatibility only: leave `luaActorLoader: false` and set
  `patchScript: true` to opt into patching an unshared BPModLoaderMod script.
  A hardlinked script is refused, so Vortex's deployed file stays intact.

The Lua path reads an atomically generated manifest before Lua startup. It
does not make a multi-actor world spawn transactional: a failed ModActor is
logged and other packages continue. It is client-side only; dedicated-server
authoritative registry/data loading remains RuneSchema's native responsibility.

Before considering release, verify startup, world entry, menu return, world
re-entry, and restart with one and then multiple LogicMods; compare the
`[LOGIC-PAK][RUNE-LUA]` messages against native startup and check for duplicate
ModActors. Confirm Vortex redeploy does not flag BPModLoaderMod's `main.lua`.
