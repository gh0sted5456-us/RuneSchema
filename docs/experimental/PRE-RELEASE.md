---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

# Pre-release · 0.8.1.0

!!! info "Available for testing · pending promotion to stable"
    The November 14 preview includes faster Item Bay search, live server policy updates, and the experimental Steam Pak Loader. Stable RuneSchema is **0.8.0.3**.

<div class="rs-actions">
  <a class="md-button md-button--primary" href="https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0-pre.20261114.3/RuneSchema-0.8.1.0-preview-20261114.3.zip">Download complete preview</a>
  <a class="md-button" href="../INSTALLATION/">Installation</a>
</div>

The complete package includes RuneSchema, Helpy, bundled plugins, and matching
Lua adapters. Install the pinned UE4SS build separately and preserve your
settings, mods, and saves when updating. Use **Exit Helpy** before replacing it.

## Pending changelog

### Item Bay, cache, and Helpy

- Browse cached items offline, filter by source, drag selections to the cart, or send an item directly while the cart is populated. Cached searches reuse a prepared index. Live searches reuse the mounted item index and save verified discoveries for later searches.
- Right-click an item to recapture its icon from the game or refresh icons for that item's mod. Mod-wide refresh requires **Keep icons between launches**.
- Recall downloaded item records and icons reliably. Failed learned-item writes retry in order, with diagnostics for failed writes and dropped observations. Cache reads, unchanged saves, live-result merges, and host-worker transport reuse do less repeated work.
- Use the two-pane desktop and overlay layouts with contextual keyboard and controller glyphs, an in-app Help guide, and improved navigation across settings, tools, Item Bay and maps. The overlay reacquires controller input after focus returns. Helpy launched from the desktop can stay open when the game closes.
- Explore atlas close-ups, coordinate overlays, player centering and named Dragon Gate travel to Rasthin’s Rest. The welcome screen supports rotating platform and theme-colored logos and optional artwork crossfades.
- Recover Helpy render targets after rejected resizes, improve desktop graphics and plugin diagnostics, and reduce overhead in the Windows Shipping build.

### Servers and multiplayer

- Use the CL260223 native dedicated-server filter binding. The host sends [item-grant and access policy](SERVER-ACCESS.md) on connection and pushes changes to connected clients. A grant limit of **0** is unlimited. Existing configured limits still apply.
- Compare mods with the server, configure PIN access, and save player authorizations. Experimental [Item Delivery](ITEM-DELIVERY.md) tracks grants per character and waits for the matching save acknowledgement.
- The bundled [RSNetworking policy plugin](MULTIPLAYER-PLUGIN.md) adds host controls for opted-in actors. The [network bridge](NETWORK-CAPACITY.md) supports larger payloads within per-route and per-peer budgets and reduces repeated network and Lua relay work.

### UE4SS, mod tools, and runtime

- The bundled [RuneSchema.UE4SS.Bridge](UE4SS-SISTER-CONTENT.md) lets enabled standalone UE4SS mods register their own content and opt into appearance and namespaced mod-data networking.
- The experimental **RuneSchema Pak Loader** author kit lets an existing native UE4SS mod mount its local `paks/` folder at startup. Multiple opted-in mods can share the same game startup. Download the Steam author kit separately and follow the bootstrap steps. The kit contains the helper DLL, headers, config, and notices. [Pak Loader guide](UE4SS-PAK-LOADER.md)
- RuneSchema and UE4SS share a Compact view with mod metadata, licenses, cache building, and loader controls. Mod Menu 2 keeps valid pages available when another mod reports a schema error and supports offline editing for eligible settings. Mod Menu 2 and Advanced Tooling remain optional and key-gated.
- Redirected AppData and character save roots work with the pinned UE4SS host. Save cleanup scopes its changes, reports removals, and backs up affected records where supported. World recovery is opt-in.
- Startup and recipe preparation reuse discovery work, tolerate malformed plugin order entries, and report advisory UE4SS version notices. Temporary reflected map and array values release nested allocations. Experimental authoring includes cooked world components, typed templates, numeric providers, guarded host commands and shared HUD services.

[Dated development history](CHANGELOG-0.8.1.0.md) and [feature guides](index.md)

## Verification

All 86 runtime files passed archive SHA-256 verification. Nine Helpy tests, two core tests and eleven Pak Loader contract checks passed. On Steam, two native mods and RuneSchema each mounted their PAK and IoStore content once. An isolated CL260223 dedicated server listened, loaded its world and requested native filter setup. The runtime matches the desktop Steam, desktop Game Pass, and dedicated Steam installations by hash.

[Release notes and checksums](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0-pre.20261114.3)

## Feedback

[Report a bug on Nexus](https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs)
with the build, platform, reproduction, and relevant diagnostic codes.

## Known limits

Live joins and rejoins, in-game search latency and icon recapture, Proton and Wine, the physical controller matrix, live Rasthin's Rest arrival, and multiplayer restart behavior remain unverified. A controller fixture still reports D-pad focus crossing. Item Delivery needs live grant and save acknowledgement checks. Pak Loader testing covers Steam clients. Game Pass and dedicated-server Pak Loader behavior still need verification.

The [documentation promotion procedure](../DOCUMENTATION-CHANNELS.md)
describes how these guides and notes move to stable.
