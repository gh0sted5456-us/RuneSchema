# Included Lua UE4SS add-ons

RuneSchema 0.8.0.0 supports complete Lua-based UE4SS add-ons inside an enabled
RuneSchema mod. Like `paks`, `ue4ss` is a container for separate packages:

```text
RuneSchema/mods/MyMod/
  ue4ss/
    MaxCape/
      Scripts/main.lua
      Scripts/helpers.lua
      config.txt
      modmenu.json
    AnotherLuaAddon/
      Scripts/main.lua
      settings.ini
```

Keep each complete package intact. Names must be unique across included
packages and ordinary UE4SS mods. Never install a second copy under
`UE4SS/Mods`; a collision is rejected rather than silently replacing it.

## Player setup

Keep your original **unmodified UE4SS.dll**. RuneSchema updates are framework-only;
the previously produced managed-host bundles are retired and must not be used.
Lua-only packages use RuneSchema's bootstrap. Native DLL add-ons are outside
this feature's scope and must be installed through stock UE4SS separately.
RuneSchema skips an entire native/hybrid package with a clear diagnostic;
it never silently runs only its Lua half or manually calls native entry points.

Enable the parent as usual in `RuneSchema/mods/runeschema.txt`:

```text
MyMod: 1
```

`MyMod: 0` disables all its packages on the next launch. Restart after adding,
removing, enabling or disabling packages; this is
not hot reloading. Changing worlds does not start a package twice.

## When companion scripts run {#lifecycle-boundary}

RuneSchema does not replace or patch the UE4SS runtime. Stock UE4SS performs its
native mod discovery before RuneSchema can announce nested packages. Native
loading is therefore not part of this feature. The Lua bootstrap
does not create independent UE4SS ModRef instances or independent host-owned VMs.

Package configs, scripts, metadata and dependencies remain at their original
paths. Discovery does not rewrite or prune them. Native mods still need a
compatible UE4SS SDK and game version; bundling cannot make an incompatible or
unsafe DLL safe. Host errors are reported per package, not described as success.

RuneSchema uses the stock Lua-start callback to expose enabled Lua packages as
additional entries in `IterateGameDirectories()` under the current runtime's
`Mods` table. Each entry is the complete original directory node, including its
scripts, config, metadata and asset folders. Its absolute paths still point to
the nested package. This lets Mod Menu discover a package such as
`Bootylicious/ue4ss/MorphSliders` without patching Mod Menu or moving its files.

This adds no polling or extra recursive disk scan. Native entries and return
values remain intact; existing sibling names win, and disabled parents and
unsupported native/hybrid packages are not presented. Restart after updating
RuneSchema so its Lua-start integration can run before the settings scan.
Tools that bypass the directory API and only enumerate physical top-level
folders still need their own nested-package support. An unavailable native
directory listing is passed through unchanged, including Mod Menu's fallback.

## Legacy Lua companions

The old `MyMod/ue4ss/scripts/main.lua` layout remains supported. Do not mix
direct entry points with packaged child mods in the same container. Legacy Lua
keeps the compatibility bootstrap and `RuneSchemaUE4SS.Current` context.

Packaged Lua uses the RuneSchema bootstrap: helpers and globals are
isolated per package, and `RuneSchemaUE4SS.Current` supplies `Owner`, `Name`,
`Root` and `Scripts`. It is not a separate host-owned mod and does not have its
own `ModRef`. Code requiring independent VM/ModRef behavior is not a drop-in
RuneSchema add-on: adapt it to the provided context or install it directly in
stock UE4SS. Config files remain at the original package path. This context is
namespace isolation for compatibility, not a security sandbox.

## Diagnostics and trust

Lua `[OK]` confirms that the package entry script ran; it does not prove all
gameplay callbacks work. `[REJECTED]`, `[NATIVE-UNSUPPORTED]` and Lua `[ERROR]`
explain failures. Retired host-owned manifest rows are rejected rather than
being treated as successfully started.
The session-scoped generated manifest is diagnostic state, not user config.
Stale manifests cannot start disabled packages on another launch.

Only install trusted Lua packages: they execute code. Packages are not
automatically networked. Install the same required content on server and clients,
keep authority server-side, and use documented game/RuneSchema networking.
For cooked Blueprint actors instead, see [LogicMods](PSEUDO-LOADERS.md).

