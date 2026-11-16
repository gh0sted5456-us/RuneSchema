---
channel: experimental
promote_to: modders/UE4SS-SISTER-CONTENT.md
---

!!! info "Experimental · 0.8.1.0"
    Use a matching RuneSchema Core and the `RuneSchema.UE4SS.Bridge` plugin.

# Register content from a standalone UE4SS mod

Keep the mod under an active UE4SS Mods directory and enable it through UE4SS.
UE4SS starts and unloads the DLL as usual. RuneSchema reads the registration
script before Unreal scans for PAKs, so the DLL and normal Lua scripts can keep
their own startup sequence.

```text
Mods/
  SisterContentExample/
    dlls/main.dll
    runeschema/
      register.lua
      registry/example.json
      paks/
      assets/
      raws/
```

Put this call in `runeschema/register.lua`.

```lua
RuneSchema.RegisterContent()
```

The registration script has its own Lua state with the registration function
available. Keep UE4SS API calls in the mod's usual entry points. The content
root and registry owner come from the UE4SS mod folder. Repeated calls enroll
the same root once. RuneSchema reads sister content after its ordinary mods,
in folder-name order. A same-named ordinary RuneSchema mod takes precedence.
Restart Dragonwilds to pick up changes to the registration or cooked packages.

Under `runeschema/`, use the same content folders and file schemas as an ordinary
RuneSchema mod. Include all matching container parts under `paks/`.
Both `raw/` and `raws/` are accepted. If both exist, the loader uses `raw/`.
Enable Advanced Logging to see the root registration and PAK directories in
`RuneSchema.log`.

## Build the example

The repository's `docs/examples/SisterContentExample` folder contains the
UE4SS DLL source, registration script and a small registry declaration. Build
the `SisterContentExample` target in the native RuneSchema build with its pinned
UE4SS SDK. Place the generated `examples/SisterContentExample/dlls/main.dll`
and the example's `runeschema/` folder and ownership notice into a UE4SS mod
folder named `SisterContentExample`. Enable both RuneSchema and the example.
