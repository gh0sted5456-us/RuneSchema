# RuneSchema and Helpy 0.8.0.2

October 25, 2026. [Download the complete runtime](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.0.2)
· [Changelog since 0.8.0.1](CHANGELOG-0.8.0.2.md).

Both `RuneSchema/dlls/main.dll` and
`RuneSchema/plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll` are freshly built
from the 0.8.0.2 source. The release includes the earlier source-only 0.8.0.1b
registry, save-cleanup and cart fixes, plus the current cache improvements.
The `0.8.0.2` tag identifies the release source; main and experimental contain
the same authored release files.

## Install and contents

Use the full `RuneSchema-0.8.0.2.zip` with your existing stock UE4SS installation.
Close the target game/server before updating. Replace the runtime files while
preserving your mods, personal settings and AppData profiles/caches.

The ZIP retains one cleaned `RuneSchema/` root, the compressed DLL pair,
bundled bridge content/manifests, Lua helpers, defaults and consolidated legal
notice. It does not include UE4SS.dll, third-party mods, player saves, generated
mappings, debug symbols, the author tool's dependencies or a baked AppData cache.
Optional artwork and the base cache remain separate explicit downloads.

Packaging removes an identical duplicate RegistryBridge container set from
RSNetworking and restores the UE4SS enable marker, dedicated-server settings
helper and application icon. Helpy is enabled; automatic opening remains off
in the supplied defaults. Existing user settings take precedence.

## Cache updates

The GitHub cache catalogue can update independently of this DLL release.
Exact-build/cook matches admit compatible records and pictures; changed cooks
may reuse native pictures as display-only references. Current object paths,
ownership, item IDs and gameplay operations still require native checks.
The published Steam hotfix bundle contains 2,903 records and 4,246 pictures;
coverage remains partial. See [cache bundles](CACHE-BUNDLES.md).

The [offline cache builder](modders/offline-helpy-cache.md) is optional author
tooling. It runs only when invoked and is not added to normal game startup.

## Verification

`RuneSchema-0.8.0.2-verification.json` records exact archive/DLL hashes, source
identity, automated test counts, compression round-trip checks and package
inventory. `SHA256SUMS.txt` covers the downloadable ZIP and verification file.
The package is locally built using the existing pinned SDK libraries; it is
not represented as a GitHub Actions compiler artifact. Compiler timestamps
and dependency environments can prevent bit-for-bit reproduction.

Automated tests and isolated plugin startup are not live gameplay or frame-time
measurements. Historical releases and tags remain intact.

Validation passed 243 native contracts and all 103 Helpy contracts, plus the
compressed Helpy startup checks, 286-symbol UE4SS import checks against Steam,
Game Pass and server hosts, and strict documentation/link checks across 108
pages. Two cache fixtures required shorter temporary directories to stay
within Windows path limits; no production behavior was changed for that run.
