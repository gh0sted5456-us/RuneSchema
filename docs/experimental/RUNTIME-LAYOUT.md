---
channel: experimental
promote_to: RUNTIME-LAYOUT.md
---

!!! warning "Experimental development candidate"
    The folder restructure requires a matching rebuilt core, bridge and Helpy. It has not replaced a published package.

# Runtime folders

```text
RuneSchema/
  dlls/
    main.dll
    plugins/
      RSNetworking/
      RuneSchema.RegistryBridge/
      RuneSchema.QuestItems/
  Helpy/
    Helpy.exe
    Helpy.dll
    SDL3.dll
    RuneSchema.Helpy.Vulkan.dll
    plugin.json
    assets/
    settings/
    plugins/
      RuneSchema.HUD/
        plugin.json
        dll/RuneSchema.HUD.dll
      RuneSchema.ModMenu2/
        plugin.json
        ModMenu-MIT.txt
        dll/RuneSchema.ModMenu2.dll
  mods/
  settings/
    settings.ini
    plugins.txt
```

UE4SS's enable marker, scripts and minimal Mod Menu definition remain required runtime files. The installed Helpy executable runs with or without the game. Desktop shortcuts point to that executable.

Core services live under `dlls/plugins`. Helpy's bridge and feature bundles live under `Helpy`. Native HUD adapters still load inside the game. Plugin IDs, API version, service names and mod-owned `/ue4ss/hud.json` bindings remain unchanged. Mod Menu 2 is a native Helpy-process plugin (`Host: "helpy"`); its shared renderer and narrow game adapter retain existing controls. Its activation key can live in `Helpy/plugins/RuneSchema.ModMenu2/modmenu2.key`; `Helpy/modmenu2.key` remains supported.

AppData keeps the same runtime identity, caches, profiles, notifications and window state. Installed mod folders remain under `mods`. Helpy's bundled settings and theme live under `Helpy/settings`.

## Redirected AppData

The trusted `LocalAppData/RSDragonwilds/RuneSchema` root resolves to its physical
directory at startup. Redirected user profiles, game-data folders and whole
RuneSchema data roots remain usable without moving or copying their contents.
Core and Helpy use the same resolved root. Links below that root, including
redirected cache or settings children, remain guarded; an error names the exact
blocked path.

A startup log showing UE4SS `f6d5f942` already matches the pinned compile SDK.
`DID-NOT-START` followed by a redirected-path error concerns data-path setup;
changing the UE4SS version does not address that failure.

## Transition

Use a complete matching candidate with the game and Helpy closed. Preserve existing Helpy settings, themes and activation keys when moving the old plugin folder. Existing owned desktop shortcuts are refreshed at the next launch; custom shortcuts are preserved. Already enrolled Vulkan startup bindings follow the relocated HUD adapter.

Legacy `plugins/<PluginId>` installations remain readable. New locations win when both locations contain the same plugin ID, and only one copy initializes. New packages store shared plugin order in `settings/plugins.txt`; old `plugins/plugins.txt` remains the fallback.

Package staging, license collection, repair and hotfix validation recognize the new folders. Repair retains its protected settings, saves, cache and user-mod exclusions. Historical packages keep their original paths and remain supported.

## Validation

Path/catalog, package staging, repair, shortcut and standalone startup checks cover the change. Platform gameplay and native overlay acceptance must be recorded separately before installing this layout as an update.
