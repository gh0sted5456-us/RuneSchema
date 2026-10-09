# RuneSchema

Runtime schema and data-editing framework for **RuneScape: Dragonwilds**.

> **Documentation Website:** https://gh0sted5456-us.github.io/RuneSchema/

[Open the RuneSchema documentation →](https://gh0sted5456-us.github.io/RuneSchema/)

## Downloads

The repository is source-first. Starting with the next release, RuneSchema ships
one compiled package containing its bridge plugins. Storefront UE4SS bundles,
build dependencies, and hashes remain separate GitHub Release assets:

https://github.com/gh0sted5456-us/RuneSchema/releases

The RuneSchema runtime detects Steam/GOG and Game Pass/WinGDK
automatically. UE4SS storefront packages are distributed separately so users
do not repeatedly download the same runtime with every RuneSchema source
snapshot.

## Building

Run `Build RuneSchema.bat` from the repository root.

The builder downloads the small pinned build-dependency bundle on first use,
verifies its SHA-256 hash, and caches it under `.cache/`. CMake/UE4SS source
dependencies continue to be fetched by the normal build process. Generated
build output is written to `dist/` and is not source-controlled.

Authoring, loader, Unreal + RuneSchema, compatibility, API, and release
documentation is maintained on the public documentation site above.

## Community and license

RuneSchema is developed and maintained by the **RSDW Modding Community**.
Credited members: **Jonesing4Space, NuLLZz, Snorkles, and CHP**.
Published under **gh0sted5456-us**. See [community credits](AUTHORS.md).

Copyright (c) 2026 RSDW Modding Community. Original software and documentation
owned by, or licensed with authority by, the community are available under the
[MIT License](LICENSE). See [license scope](LICENSING.md) and
[third-party notices](THIRD_PARTY_NOTICES.md) for component-specific terms.

PalSchema foundation by Okaetsu. UE4SS and other dependencies retain their
original copyright and license notices.
