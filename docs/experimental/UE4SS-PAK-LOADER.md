---
channel: experimental
promote_to: modders/UE4SS-PAK-LOADER.md
---

# RuneSchema Pak Loader

!!! info "Experimental · Dragonwilds Steam"
    Built against UE4SS `f6d5f9425a31f3e3a4fd185b40148f5aa4d81a44`.
    Game Pass and dedicated server support are unverified.

[Download the Steam author kit](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0-pre.20261114.3/RuneSchema-PakLoader-Steam-CL260223.zip)

Use the loader in an existing UE4SS C++ mod. It mounts cooked content at game
startup. The mod can run with UE4SS alone. If RuneSchema is also enabled, use
a RuneSchema Core build with the companion mount API included in this source.

```text
ue4ss/Mods/ExampleMod/
  dlls/main.dll
  runeschema-pakloader.dll
  config.ini
  paks/
    Example_P.pak
    Example_P.utoc
    Example_P.ucas
```

In `config.ini`, set the name to the exact mod folder.

```ini
[RuneSchemaPakLoader]
ModName=ExampleMod
```

Keep PAKs directly under `paks/`. Include matching `.utoc` and `.ucas` files
alongside each IoStore PAK. Conventional PAK-only cooks can ship a lone
`.pak`. Keep each cooked set at a single path and enable the mod as usual in
UE4SS. Restart the game after changing the config, enabled state, or content.

## Bootstrap from your existing DLL

UE4SS looks in `dlls/` for the mod's native entry point. Add the supplied
`Bootstrap.h` and `PakLoaderApi.h` to your native source, then call from your
mod's `on_cpp_mods_loaded()` override.

```cpp
#include "Bootstrap.h"

// Put this override in your existing CppUserModBase subclass.
void on_cpp_mods_loaded() override
{
    static const int moduleAnchor = 0;
    const auto result = PS::PakLoader::StartFromMod(&moduleAnchor);
    // Log result through your mod's usual logger.
}
```

The helper validates its own folder and config, then submits that folder's
`paks/` to Unreal's startup scan. `Ready` means the directory was enrolled.
Check the game log's `Mounted Pak file` and `Mounted IoStore container` lines
to confirm the cook mounted. Other results are `InvalidConfig`, `NoPaks`,
`InvalidContainers`, and `MountUnavailable`.

Build the `RuneSchemaPakLoader` CMake target for the helper. The optional
`pak-loader-fixture` target demonstrates the native entry point and bootstrap.
For Jonesing4Space mods, include the approved `license-jonesing4space.txt` at
the mod root and keep any separate third-party notices.
