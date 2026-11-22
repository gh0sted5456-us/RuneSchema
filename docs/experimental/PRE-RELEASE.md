---
channel: experimental
promote_to: RELEASE-1.0.0.md
---

# RuneSchema 0.9.0.1 Pre Release

<div class="rs-actions">
  <a class="md-button md-button--primary" href="https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.9.0.1-pre.20261117.1/RuneSchema-0.9.0.1-preview-20261117.1.zip">Download RuneSchema</a>
  <a class="md-button" href="../INSTALLATION/">Installation</a>
</div>

This prerelease packages RuneSchema and Helpy for Dragonwilds CL260223. Install the pinned UE4SS build separately. Close the game and choose **Exit Helpy** before updating. Keep your settings, mods, saves, and activation keys.

<span id="pending-changelog"></span>

## Changes since 0.8.0.3

### 0.9.0.7: Wiki, plugin tiles, Grand Exchange support, character sheet · November 20–21

This candidate collects everything since 0.9.0.3.

**For players**

- New `RuneSchema.Wiki` plugin: a **Wiki** tile that shows the Dragonwilds wiki inside Helpy's window and its game overlay. Type `rs wiki <name>` in the game console to open a page. See [Dragonwilds Wiki in Helpy](HELPY-WIKI.md).
- Plugin entries such as Grand Exchange are tiles on Home with their own icon and description, and sidebar rows with their icon, instead of a plain button above the tiles.
- Plugins can post into Helpy's notifications, with an in-game popup and an **Open** button for their own page. Notices appear under **Notifications → Plugins**; Grand Exchange can use this for sale alerts.
- Grand Exchange opens as its own workspace inside Helpy, with **Return to Helpy**. See [Grand Exchange integration](GRAND-EXCHANGE.md).
- The character sheet has four pages (Inventory & loadout, Appearance, Skills, Save data) and one draft bar for Review & export and Discard changes. Add Item Bay items to the draft with a quantity. See [Character sheet](CHARACTER-SHEET.md).
- The Item Bay shows why deliveries to your own character are off or paused by a mod, with **Turn deliveries on** for your own switch, and checks again before sending. See [Helpy delivery flags](HELPY-DELIVERY-FLAGS.md).
- Item search understands `OR` / `|`, `-word` / `NOT`, parentheses and "phrases", in the Item Bay, gear pickers and spawn tools.

**For mod and plugin authors**

- A `Host: "helpy"` plugin can ship as one folder under `Helpy/plugins/<Id>`: an optional in-game half (`GameEntryPoint`), RuneSchema content (`Content: true`), and calls from its Helpy half to services its in-game half registered.
- Owned plugin events (`helpy.plugin.event`) let an in-game half notify its own Helpy half and open its own route.
- An interrupted plugin update is recovered before plugin discovery, and the externally updated plugin folders are not scanned if the update lock can't be taken.
- New Helpy plugin UI: Home tile fields, a text box and host-owned web pages. Plugins can also post notices with `helpy.notify`. See [Navigation API](GRAND-EXCHANGE.md#navigation-api) and [Plugin notices](GRAND-EXCHANGE.md#plugin-notices).
- A mod can sign a time-limited **character exception** to its own delivery pauses. See [Character exceptions](HELPY-DELIVERY-FLAGS.md#character-exceptions).
- Core service `runeschema.item.snapshot`: a read-only, lossless record of one live item, for in-game plugin halves.
- Plugin server requests (in development): a plugin's in-game half can ask the server it's on to run the same plugin's server half, which can put items into a chest or inventory with `runeschema.item.grant`. Grand Exchange uses this to deliver on any server that has it installed. See [Plugin server requests and item grants](PLUGIN-SERVER-REQUESTS.md).
- Core service `runeschema.item.destroy` (in development): removes one exact whole stack through the game's own destroy call, for in-game plugin halves that declare `item.destroy`. See [Exact item destroy](ITEM-DESTROY.md).
- Name-only engine hooks share one dispatcher, which cuts per-call overhead.

### Helpy data in AppData, Achievements plugin · November 19

- Helpy keeps its own files in `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core`: settings, theme, item groupings, downloaded artwork, your own backgrounds and fonts, and a copy of the licence bundle. The installed mod now holds only runtime files, plugin DLLs and the single licence bundle in `settings/licenses`.
- Runtime folders are named for the game binaries: `Win64` for Steam and Epic, `WinGDK` for Game Pass, and `Server` for a dedicated server. The first start moves the older `steam`, `epic`, `gamepass`, `server`, `HELPY`, `runeschema.profile` and `health` folders once, without copying or deleting. A folder in use stays where it is until a later start. See [Runtime folders](RUNTIME-LAYOUT.md).
- The dedicated-server apply script is built into Helpy.exe and written to `core/HELPY/tools` when you copy the apply command.
- Achievements are a Helpy plugin, `RuneSchema.Achievements`. It finds `achievements.json` in UE4SS mods as well as RuneSchema mods, so any mod can award achievements. Turning the plugin off in `plugins.txt` turns achievements off and keeps earned points. See [Mod achievements](../MOD-ACHIEVEMENTS.md).

### Item Bay search fixes · November 17

- Added **Stop search** to the Item Bay controls. Stopping keeps the displayed results, selection, and delivery cart available and cancels further search requests.
- Fixed repeated searches when the Inventory Workbench or Character Studio Item Bay adopted a saved source, category, or **Show Mod Family** filter. Filter changes are applied and shared before the grid renders.
- A stopped search stays stopped when a saved filter changes. Rejected requests wait for an explicit retry instead of being resubmitted every frame.
- Search progress stays in Item Bay, and filtered Item Bay results stay out of Helpy's shared tool snapshot. Gear pickers use the unfiltered browse preview.
- Late replies from cancelled searches are ignored, and stopping an item search leaves AI and resource catalogue requests available.

### Helpy desktop, overlay, and settings

- Helpy runs as a separate desktop app with a system tray icon and a shortcut for each installed runtime. A desktop-launched session can stay open for offline browsing after the game closes. A game-launched session can close with its attached game or server.
- Settings, Quick Settings, Tools, and mod management share a two-pane layout with a category rail and independently scrolling controls. The rail can collapse or stay pinned. Startup, overlay, shortcuts, performance, and cache preferences are available from Settings.
- The themed game overlay has adjustable opacity, size, and interface scale. The desktop window and tray remain available. New settings default to native presentation, and existing window preferences are preserved.
- The overlay includes Direct3D 11, Direct3D 12, OpenGL, and opt-in Vulkan presentation paths. Changes address rejected resizes, 10-bit SDR target handling, Direct3D 12 frame submission, hybrid-GPU adapter changes, and stale cursor and panel pixels.
- Interactive overlays capture keyboard, mouse, and controller input and release it when hidden or disconnected. Passive startup, coordinates, and notification displays leave game input available.
- The welcome screen supports rotating platform and theme-colored RuneSchema logos, adjustable timing, and artwork crossfades. Windows reduced-motion settings keep the splash static. The in-app Help guide covers controls and troubleshooting.

### Controller and keyboard navigation

- Helpy adds automatic input-mode detection and selectable Xbox, PlayStation, and generic button glyphs. Controller input gives the welcome screen and Home an initial selection without requiring a mouse click.
- Triggers switch main sections, bumpers switch panes or tabs, and the D-pad moves focus. The left stick scrolls or pans. The right stick can move a pointer, click, and drag. Back returns through dialogs and panels.
- Controller actions cover Item Bay selection and quantity controls, cart drops, mod actions, Settings, Tools, and map panning and zoom. Text fields include an on-screen keyboard, including the server PIN field.
- The controller menu shortcut is configurable. Supported PlayStation touchpads have an optional pointer mode. Native overlay input is reacquired after focus returns.

### Item Bay, filters, and caches

- Interactive live checks continue sooner between slices. Learned-item writes preserve the newest observation, skip redundant settled repeats, and coalesce cached-search index rebuilds across live search slices.
- Item Bay can browse saved items and icons offline. The search bar groups source, category, family, and display-name filters. The display/internal-name switch preserves the current query, selection, and asset identity.
- Cached searches reuse prepared indexes and unchanged rows. Live searches reuse the mounted item index, and discoveries with valid asset identities are saved for later searches. Live work and cancellation run off Helpy’s UI thread.
- Downloaded records and icon packs remain available to the cache. Learned-item writes retry in order after a failure. Diagnostics report failed writes and discarded observations. Unchanged cache writes and repeated source-plan copies are reduced.
- **Refresh search** reruns the query with the active filters. Right-click an item to request a new in-game icon capture or refresh icons for its mod. Mod-wide icon refresh requires **Keep icons between launches**.
- Search and cache controls retain cached results during updates. Source grouping and name recovery preserve readable labels and distinguish different assets with the same display name. Cooked equipment types help categorize weapons, armor, and ammunition.
- Select individual items or groups and move them to the cart. **Send to player…** opens a separate quantity and recipient picker for one item while preserving the staged cart.
- **Cache enabled mods** runs a sequential queue with pause, resume, stop, and per-mod results. Installed mods and vanilla files can be read in the cache worker. Cache bundles preserve portable item taglines and tips.

### Mod achievements and Gamer Points

- Mods can declare achievements and award RuneSchema Gamer Points. In **Mods**, right-click an enrolled mod and choose **View Achievements**, or open **View All Achievements** to browse every enrolled mod.
- Achievement placards show the award's points and dim locked entries. Hover to see unlock details when available. The player profile shows the total with a color-tiered points badge.
- The native HUD plays a RuneSchema logo reveal for an unlock while the game keeps focus. The award can play an optional chime, and the notification respects Windows reduced-motion settings.

### Helpy responsiveness

- Input state updates and achievement requests use a background connection to RuneSchema Core. Closing Helpy coordinates with pending input work, and replies from a previous attachment cannot replace the active player's achievement view.
- Catalog responses reuse a shared snapshot so Helpy's transport can read a consistent catalog without copying the full result while the catalog lock is held.

### World map, HUD, and presentation

- The world atlas adds selectable zone close-ups, transport outlines, a coordinate view, and player centering. Scorned Wilderness has its own view. The map includes a named Dragon Gate destination for Rasthin’s Rest.
- Helpy adds optional startup artwork and progress, a coordinate HUD, and a HUD layout preview for minimap, coordinates, notifications, and loading artwork. HUD positions can be saved from the preview.
- Mods can use the shared HUD and presentation interfaces, declare input bindings, and supply an optional owner-only presentation handler for a registered consumable.

### Dedicated servers and multiplayer

- RuneSchema adds the CL260223 native dedicated-server filter binding. In an isolated Steam dedicated-server run, the server opened its game port, loaded the world, and requested native filter setup.
- The authority sends item-grant and access policy when the client connects and pushes policy changes to connected clients. A limit of **0** means no server total per grant request. Existing configured limits still apply.
- Hosts can configure world PIN access with separate self-delivery and self-teleport permissions, or save grants by character GUID. The server checks request identity, item IDs, quantity, and destination. Correlated results distinguish an accepted request from a completed action.
- **Mods → Server mods** shows the host’s enabled mods and versions beside local entries, including missing, disabled, version-different, and client-only entries. Authors can supply a download-page link through mod metadata.
- The bundled RSNetworking policy plugin adds opt-in actor policies and larger network payload budgets. The network bridge adds declared character-appearance snapshots and namespaced mod-data channels.
- The building registry checks compatibility before modded placement by comparing network ID fingerprints while retaining the verified native ID prefix.
- Item Delivery adds host-configured delivery lists with **Off**, **Deliver Once**, and **Always Deliver**. Its receipt logic binds deliveries to character identity and records completion on the matching save acknowledgement.

### Mods, authoring, and content registration

- Named `$RuntimeWidget` and `$RuntimeUI` rules filter events before walking the owner hierarchy. Wildcard rules and UI teardown retain their normal handling.
- Helpy's **Mods → UE4SS mods** list shows **J4S PAKLoader** and **Maxx's Mod Menu** badges on cards and in Compact view. Refresh the inventory after adding a descriptor.
- RuneSchema and UE4SS share a compact mod-management view with metadata, license inspection, next-launch loader controls, and cache actions. The UE4SS inventory distinguishes external packages and RuneSchema-owned packages.
- The bundled **RuneSchema.UE4SS.Bridge** lets an enabled standalone UE4SS mod declare and register RuneSchema content, including appearance and mod-data declarations.
- **Mods → Mod Menu 2** adds a key-gated Helpy editor with legacy callbacks, offline editing for supported settings, optional character tabs, and template import and export. Supported controls include sliders, radial choices, and RGB/RGBA colors. A schema error stays on its own page so valid pages remain available. The existing external Mod Menu keeps precedence when enabled.
- The runtime includes optional, key-gated Advanced Tooling. Helpy’s authoring tools build and update caches, and the repair tool verifies the matching package, backs up replacements, and preserves user mods, settings, saves, and caches.
- Cooked registry support expands to table extensions, skill declarations, world components, and guarded host console commands. Mods can declare local profiles, typed templates, and numeric providers. The settings processor handles up to 1,024 combined schema settings, tab keys, and template entries.
- Cooked world policies add hooks for building support, placement and dismantling, player phasing, material requirements, farming, resource yields, player effects, and enemy spawning. Material policy calls preserve native unlock, quantity, station, and authority checks.

### Saves, startup, and diagnostics

- RuneSchema reads and parses mod JSON ahead on a bounded worker pool when the host has at least four hardware threads. Loader callbacks and diagnostics still run in file order on the calling thread.
- Automatic reflection mapping capture now requires a fresh request from RuneSchema Core. Steam and Game Pass both reused their cached mappings on the next startup without writing a new dump.
- Helpy’s Save Recovery view lists character snapshots with their reason and active mod metadata. World snapshot and restore controls are opt-in. Cleanup stays scoped to the selected save and uses verified backups on supported paths.
- Character copy, import, and export follow redirected save roots. Core and Helpy share the resolved AppData root while guarding nested paths. Optional world archive errors are reported separately from verified cleanup backups.
- Startup discovery reuses mounted-directory results, ownership checks, and ready recipe references. Registry merging reduces duplicate copies. Recipe prefetch stays bounded, malformed plugin order entries leave other plugins available, and temporary reflected map and array values release nested allocations.
- The core and Helpy reduce repeated transport work, idle cache copies, registry notices, and diagnostic traffic. Helpy recovers its previous render target after a rejected resize and reports the window, graphics error, and source plugin where available.
- Diagnostics reset with each attached session and include resource sampling, startup timings, and bounded Lua and UE4SS activity reports. Helpy adds a persistent notification inbox, stable-release and hotfix notices, and a clipboard bug report.
- The runtime groups core plugins under `dlls/plugins` and the desktop app, HUD, and Helpy plugins under `Helpy`. The complete ZIP pairs the core, Helpy, cooked bridges, Lua adapters, and licenses from the same build.

## Verification

The release includes a SHA-256 verification manifest covering all 86 files in the runtime ZIP. The Helpy desktop and preview builds completed, and all ten targeted Windows checks passed, including the search-loop, cancellation, transport, and three window-size fixtures. In-game verification of this search update remains pending.

[Release files and checksums](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.9.0.1-pre.20261117.1)

## Feedback

[Report a bug on Nexus](https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs) with the build, platform, reproduction, and relevant diagnostic codes.
