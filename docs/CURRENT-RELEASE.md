# RuneSchema downloads and source updates

The current stable release is **0.8.0.3**, published October 28, 2026. Its source
is on `main`, identified by the `0.8.0.3` tag. The `experimental` branch contains
newer integration work, documented in its own [Experimental area](experimental/index.md).
Source updates do not automatically replace the release package.

[Download 0.8.0.3](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.3/RuneSchema-0.8.0.3.zip)
· [Installation, defaults and verification](RELEASE-0.8.0.3.md)
· [Itemized changes since 0.8.0.2](CHANGELOG-0.8.0.3.md).

The download includes one core DLL for Steam/Epic, Game Pass and dedicated
servers, plus Helpy and bundled bridge content. Both DLLs are compressed.
Use your existing stock UE4SS. The verified package is built locally rather
than supplied by the hosted compiler workflow.

## What changed

- Selected-character/world cleanup at entry is the default. Preview does not
  authorize pruning. Blanket cleanup remains an opt-in fallback.
- Quick Settings opens as a centered panel, with activation timing and optional
  Maxxfilth Mod Menu support.
- Enabled nested Lua packages are exposed through UE4SS directory discovery so
  Mod Menu can find settings without moving files or patching Mod Menu.
- Helpy adds optional FPS displays and a render limit, family-selection fixes,
  update-aware caches and independent map/artwork/filter downloads.
- Registries accept generic names and independently named shards while
  retaining owner, schema and transaction checks.

Preserve existing settings, mods, AppData profiles/caches and important saves
when updating. [Save safety](SAFE-SAVE-AND-LEDGER.md) explains cleanup and backups.
Game Pass WGS and server joins need separate live validation for the new scope.

## Guides and previous releases

- [Install or update](INSTALLATION.md)
- [Player guide](users/index.md)
- [Create or adapt a mod](modders/index.md)
- [Cooked registry declarations](COOKED-PAK-REGISTRY-MANIFEST.md)
- [Shared cache bundles](CACHE-BUNDLES.md)
- [Previous 0.8.0.2 release](RELEASE-0.8.0.2.md) and [changelog](CHANGELOG-0.8.0.2.md)
- [Earlier 0.8.0.1 release](RELEASE-0.8.0.1.md)

See the verification JSON accompanying the download for exact hashes, source
commit and test results. Automatic and isolated checks do not certify every mod
or guarantee that damaged save data can be repaired.
