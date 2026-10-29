# RuneSchema and Helpy 0.8.0.3

October 28, 2026.
[Download](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.0.3)
· [Itemized changelog](CHANGELOG-0.8.0.3.md).

This release adds selected-save cleanup, clearer Quick Settings, optional Mod
Menu integration, nested Lua discovery, FPS controls, family-grouping fixes,
update-aware caches and independent map/artwork updates.

## Install or update

1. Close the game or dedicated server normally.
2. Back up RuneSchema and important game saves.
3. Extract `RuneSchema-0.8.0.3.zip` into UE4SS Mods. Preserve existing mods,
   settings and AppData profiles/caches when asked about replacement.
4. Start the game/server and check RuneSchema and Helpy's reported versions.

The archive has one `RuneSchema/` root with compressed core/Helpy DLLs, bundled
bridge content, manifests, Lua helpers, defaults and legal notice. It contains
no UE4SS runtime, third-party mods, personal saves, debug symbols, generated
mappings or personal AppData caches. Keep your existing stock UE4SS.

## Defaults and compatibility

- Cleanup targets the character/world being entered. Preview alone does not
  authorize pruning. Existing explicit Blanket choices are preserved.
- **Blanket save cleanup (fallback)** is opt-in and remains saved until disabled.
  Choose next-entry or next-launch activation; next launch is the default timing.
  Both modes retain verified backups and section-specific checks.
- Xbox WGS is not edited as local JSON. Removed progress is not automatically
  restored. Read [save safety](SAFE-SAVE-AND-LEDGER.md) before removing content.
- Helpy uses F2 by default. Automatic opening and FPS displays are off in
  supplied defaults; existing preferences are retained. Quick Settings exposes
  the shortcut, startup preference, render limit and optional counters.
- Maxxfilth's Mod Menu is optional. Nested pages use original package paths;
  Bootylicious's page is **Morph Sliders**. Restart after updating.
- Native/hybrid DLL add-ons still use stock UE4SS installation. Registry-ID
  alignment remains experimental and disabled by default.

## Verification

`RuneSchema-0.8.0.3-verification.json` records the source commit, ZIP/DLL hashes,
file inventory and completed checks. `SHA256SUMS.txt` covers the archive and that
JSON. DLLs are locally built using the available SDK/toolchain, rather than
hosted compiler artifacts. Byte-for-byte reproduction elsewhere is not claimed.

Copied Steam saves verified preview protection, selected-only cleanup, an exact
original backup, stable re-entry and both activation choices. The final cleanup
candidate left 32 other copied files and all 33 originals unchanged.

Copied Mod Menu 2.0.2 files verified nested discovery, config reads, shared
settings and writes to the original nested config. An isolated game verified
the native Lua-start integration. UI render checks cover regular, compact and
minimum windows. The verification JSON gives final test counts and compression checks.

Live Game Pass WGS and server character joins need separate validation for the
new cleanup scope. The changes do not establish that every network hitch or
graphics crash is fixed. Existing content compatibility requirements apply.

[Previous release](RELEASE-0.8.0.2.md) · [Installation guide](INSTALLATION.md)
