# RuneSchema

Runtime schema and data-editing framework for **RuneScape: Dragonwilds**.

> **Documentation Website:** https://gh0sted5456-us.github.io/RuneSchema/

[Open the RuneSchema documentation →](https://gh0sted5456-us.github.io/RuneSchema/)

## Downloads

The repository is source-first. Compiled RuneSchema packages, storefront UE4SS
bundles, build dependencies, hashes, and installation packages are published as
separate GitHub Release assets:

https://github.com/gh0sted5456-us/RuneSchema/releases

The universal RuneSchema runtime detects Steam/GOG and Game Pass/WinGDK
automatically. UE4SS storefront packages are distributed separately so users
do not repeatedly download the same runtime with every RuneSchema source
snapshot.

## Building

Run `Build RuneSchema.bat` from the repository root.

The builder downloads the small pinned build-dependency bundle on first use,
verifies its SHA-256 hash, and caches it under `.cache/`. CMake/UE4SS source
dependencies continue to be fetched by the normal build process. Generated
packages are written to `dist/` and are not source-controlled.

Authoring, loader, Unreal + RuneSchema, compatibility, API, and release
documentation is maintained on the public documentation site above.

## Ownership and license

RuneSchema is an **RSDW Modding Community** project. Luke (gh0sted5456-us) is
the author of the community continuation; the team supports updates, testing,
and maintenance.

Original software and documentation owned by, or licensed with authority by,
RSDW Modding Community are available under the [MIT License](LICENSE).
Copyright (c) 2026 RSDW Modding Community. Third-party and historical material
retain their own applicable rights and terms; see [license scope](LICENSING.md)
and [third-party notices](THIRD_PARTY_NOTICES.md).

Historical credit: original RuneSchema by Snorkles; PalSchema foundation by
Okaetsu. These credits are not a transfer of upstream copyright.
