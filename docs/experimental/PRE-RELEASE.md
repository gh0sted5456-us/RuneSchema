---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

# Pre-release · 0.8.1.0

!!! info "Available for testing · pending promotion to stable"
    The November 14 preview includes faster Item Bay searches, persistent live discoveries, and scoped icon refresh. Stable RuneSchema is **0.8.0.3**.

<div class="rs-actions">
  <a class="md-button md-button--primary" href="https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0-pre.20261114/RuneSchema-0.8.1.0-preview-20261114.zip">Download complete preview</a>
  <a class="md-button" href="../INSTALLATION/">Installation</a>
</div>

The complete package includes RuneSchema, Helpy, bundled plugins, and matching
Lua adapters. Install the pinned UE4SS build separately and preserve your
settings, mods, and saves when updating. Use **Exit Helpy** before replacing it.

## Pending changelog

- **Item Bay search.** Cached searches do less repeated work. Helpy reuses the mounted item index for live searches and saves verified discoveries for later searches. Right-click an item to recapture its icon or refresh item icons for its mod. Mod-wide refresh requires **Keep icons between launches**.

- **Standalone UE4SS mods.** The bundled [RuneSchema.UE4SS.Bridge](UE4SS-SISTER-CONTENT.md) plugin lets enabled UE4SS mods register their own RuneSchema content. Standalone mods can also opt into RuneSchema networking for appearance and opaque mod data.
- **Multiplayer.** The bundled [RSNetworking policy plugin](MULTIPLAYER-PLUGIN.md) adds host controls for opted-in actors. The [network bridge](NETWORK-CAPACITY.md) supports larger payloads within its per-route and per-peer budgets.

- **Startup and registry.** Helpy accepts the full executable and large settings and source catalogs. RuneSchema loads the rest of the plugin catalog when an order line is malformed. Automatic mapping uses the parser’s file limit. Registry and recipe preparation do less repeated work, and wearable signature searches reuse discovery results. UE4SS version notices are advisory.
- **Helpy and input.** The desktop and overlay share a two-pane layout, contextual controller and keyboard glyphs, and the in-app Help guide. The desktop overlay captures controller input while active and reacquires it after focus returns. Controller navigation covers Settings, Quick Settings, Tools, Item Bay, and the world map, with left-stick scrolling, right-stick pointer and drag, and multi-selection. Desktop-launched Helpy can stay open after the game closes. Game-launched Helpy can exit with its attached client or server.
- **Items and travel.** Browse the Item Bay cache offline, filter by source, drag selections to the cart, or send one item directly with a populated cart. The world atlas supports zone close-ups, optional transport outlines, coordinate views, player centering, and named Dragon Gate travel to Rasthin’s Rest. The welcome screen can rotate platform and theme-colored logos and crossfade optional artwork.
- **Mods and servers.** RuneSchema and UE4SS share a Compact view. Select mods to read their available licenses, edit metadata, build caches in sequence, and opt into UE4SS loader controls. Mod Menu 2 keeps valid pages visible when another mod reports a schema error and can edit supported settings offline. Hosts can compare mod versions, require a PIN, and save player access. Experimental Item Delivery tracks grants by character and waits for the matching save acknowledgement.
- **Saves and diagnostics.** Character copies, imports and exports follow redirected save roots. Redirected AppData roots work with pinned UE4SS. Save cleanup is scoped, backs up affected records where supported, and reports what it removed. World recovery is opt-in. Diagnostics reset for each attached session and suppress repeated bridge warnings. Helpy reports resource alerts, hotfixes, and new stable releases.
- **Runtime and authoring.** Core plugins live in `dlls/plugins`. Helpy, HUD, Mod Menu 2, and Advanced Tooling use the `Helpy` tree. Temporary reflected map and array values release nested allocations on success and failure. Experimental interfaces include cooked world components, guarded host commands, mod-local typed templates and numeric providers, and shared HUD and presentation services. Mod Menu 2 supports up to 1,024 combined settings, tab keys, and template entries.

[Detailed development history](CHANGELOG-0.8.1.0.md) preserves the dated entries
from the previous candidates. [Browse the feature guides](index.md) for controls
and defaults.

## Testing and feedback

All 86 runtime files passed archive hash verification, and Helpy started from
a clean extraction. Seven targeted tests passed. The packaged binaries match
the desktop Steam, desktop Game Pass, and dedicated Steam installations. An
isolated Dragonwilds CL260223 server reached world startup and initialized
the native server filter.
[Release notes and checksums](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0-pre.20261114)

[Report a bug on Nexus](https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs)
with the build, platform, reproduction, and relevant diagnostic codes.

## Known limits

In-game search latency and icon recapture, Proton and Wine, the physical
controller matrix, live Rasthin's Rest arrival, and multiplayer restart
behavior remain unverified for this preview. A controller fixture still
reports D-pad focus crossing.
Item Delivery needs live checks for grants and save acknowledgement.
Mod Menu 2 is key-gated. World recovery is opt-in.

The [documentation promotion procedure](../DOCUMENTATION-CHANNELS.md)
describes how these guides and notes move to stable.
