# RuneSchema

Runtime schema and data-editing framework for **RuneScape: Dragonwilds**.

**RuneSchema and Helpy 0.8.0.1**, released October 24, 2026.
Read the [implemented/removed changes, line by line](docs/CHANGELOG-0.8.0.1.md)
and [exact package/defaults/verification](docs/RELEASE-0.8.0.1.md).
The release publishes the current runtime source on `main` and `experimental`,
with the `0.8.0.1` tag aligned to the source used for the rebuilt DLL pair.
The cleaned package is locally built and verified, not a hosted CI artifact.

[Download the full runtime](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.0.1).
The [0.8.0.0 release notes](docs/RELEASE-0.8.0.0.md) and historical tag remain available.

Helpy opens in its own window with F2; the old Canvas experiment is retired.
The item browser is above a staging cart: drag an item down, or right-click
and choose **Add to cart**, then confirm a grant to the selected player.
Drag a cart item back into the browser or use its context menu to remove it.
Registry, networking, and Helpy remain included plugins in one package,
with their own manifests and lifecycle.

> **Documentation Website:** https://gh0sted5456-us.github.io/RuneSchema/

[Open the RuneSchema documentation →](https://gh0sted5456-us.github.io/RuneSchema/)

## Downloads

The repository is source-first. RuneSchema ships one lean compiled package
containing the core DLL, Lua runtime, plugin DLLs/content, default
settings and consolidated legal notice. Build reports and authoring examples
belong in the repository/docs, not in the install ZIP. Storefront UE4SS hosts
and build dependencies remain separate:

https://github.com/gh0sted5456-us/RuneSchema/releases

The RuneSchema runtime detects Steam/Epic and Game Pass/WinGDK
automatically. UE4SS storefront packages are distributed separately so users
do not repeatedly download the same runtime with every RuneSchema source
snapshot.

Generated mappings live in platform-specific AppData caches. The cleaned
0.8.0.1 ZIP excludes loose UI artwork; optional artwork remains downloadable.
Its supplied settings enable detailed logging and automatic Helpy opening;
see the release notes before replacing an established configuration.

## Building

### Native menu console commands

In a loaded world, use `rs inventory open`, `rs inventory close`, or
`rs inventory toggle` in the game's console. The same actions work with
`spellbook`, `map`, `journal`, `skills`, `settings`, `buildmenu`, and `menu`
(pause). Omitting the action means `open`; `runeschema` aliases `rs`.
`rs help` lists the commands. These requests use the game's input router,
retain its pause/interaction guards, and never select Exit or change save data.
The console closes first using a configured native console key; the menu request
runs after command return. `rs closeconsole` only closes the console.
Dedicated servers have no local menus. Queued requests and results appear in RuneSchema.log.

### Compile

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
