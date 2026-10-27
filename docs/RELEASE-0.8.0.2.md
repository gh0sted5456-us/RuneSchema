# RuneSchema and Helpy 0.8.0.2

**Refreshed October 27, 2026.** This replaces the October 25 download with the
current maintenance build under the same version and release URL.
[Download the runtime](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.0.2)
· [Full changelog since 0.8.0.1](CHANGELOG-0.8.0.2.md).

The refreshed package includes the current core and Helpy DLLs, cache discovery
and persistence fixes, cart selection and presets, bounded teleport preparation,
server startup improvements, and the configurable Helpy shortcut. The matching
Lua helpers and default settings are included too.

The `0.8.0.2` tag now identifies the refreshed source. `main` and `experimental`
contain the same update. The verification download records the exact source
commit and file hashes, so you can distinguish this refresh from the original
October 25 archive even though both use version 0.8.0.2.

## Install or update

1. Close the target game or dedicated server normally.
2. Back up your existing RuneSchema installation and keep your mods, settings,
   AppData profiles/caches and game saves.
3. Extract `RuneSchema-0.8.0.2.zip` into the same UE4SS Mods location. Keep your
   established settings when your archive tool asks about replacing them.
4. Start the game or server and check Diagnostics or the startup log.

Use your existing stock UE4SS host. The archive has one clean `RuneSchema/`
root with the compressed DLL pair, shared bridge PAKs/manifests, Lua helpers,
default settings, application icon, dedicated-server settings helper and
consolidated legal notice. It contains no UE4SS.dll, third-party mods, personal
saves, generated mappings, debug symbols or baked AppData caches.

Helpy is enabled, automatic opening is off in the supplied defaults, and F2 is
the default shortcut. Change the shortcut or startup preference in Quick settings.
**Reload theme** is now a button there. Existing settings take precedence.

Optional artwork, the base-game cache and authoring tools remain separate
manual downloads. [Cache bundles](CACHE-BUNDLES.md) can update independently
of the DLL release. They help with browsing; live game checks still apply to
delivery, equipment and other gameplay actions.

## Verification

The refreshed `RuneSchema-0.8.0.2-verification.json` records the source commit,
ZIP and DLL hashes, package inventory, test results and compression checks.
`SHA256SUMS.txt` covers the ZIP and verification JSON. The superseded assets
were downloaded and hash-checked before replacement.

- 248 core tests and 109 Helpy tests passed for the maintenance DLL pair.
- Quick settings was checked at 1280×900, 960×700 and 760×560.
- Both compressed DLLs passed integrity and decompression comparisons.
- The compressed Helpy plugin boundary passed without opening a game.
- Documentation links and anchors were checked across 108 rendered pages.
- The DLLs were installed and independently hash/version-verified on Steam,
  Game Pass and the dedicated server, with backups.

The DLLs are locally built and verified, rather than GitHub Actions compiler
artifacts. Exact bit-for-bit rebuilding depends on the toolchain and dependencies.
The final hotkey build has not received an in-game gameplay test; automated
checks do not measure frame times or establish every mod's compatibility.

Registry-ID alignment remains experimental and disabled by default. Isolated
Game Pass tests covered world entry, reconnect, one item replication and one
recipe-ID notification; complete crafting and simultaneous authenticated-client
gameplay remain unverified. See the changelog for the full scope.
