---
channel: experimental
promote_to: CHANGELOG-0.8.1.0.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# RuneSchema 0.8.1.0 — experimental

!!! info "Complete candidate published · 2026-11-10"
    The [complete candidate](PRE-RELEASE.md) includes the runtime, Helpy and plugin
    follow-ups below. Older entries retain their original test-stage wording;
    their historical “unpublished” labels are superseded by this dated package.

## 2026-11-12 · Dragonwilds CL260223

- Verify the native dedicated-server filter binding for CL260223.
- Push the [item grant and access policy](SERVER-ACCESS.md) to clients on connection and when the host changes it. A limit of 0 is unlimited. Existing configured limits still apply.
- Recover Helpy render targets after resize or device changes, improve desktop diagnostics, and reduce Shipping build overhead.
- Validation covered the core and Helpy contract tests, UE4SS export checks on Steam, Game Pass and the dedicated server, and native filter initialization in an isolated CL260223 world. Live client joins still need verification.

## 2026-11-10 preview

- Add [RuneSchema.UE4SS.Bridge](UE4SS-SISTER-CONTENT.md) for content registration from an enabled standalone UE4SS mod.
- Support standalone appearance declarations and namespaced mod-data channels over RuneSchema's cooked networking bridge.
- Bundle the [multiplayer policy plugin](MULTIPLAYER-PLUGIN.md) and add [larger network payloads](NETWORK-CAPACITY.md).
- Reduce recurring network, diagnostics, and Lua relay work.

## 2026-11-09 preview

- Remove the Helpy executable ceiling and matching limits on settings, saved
  sources, plugin metadata and bundled tool scripts.
- Add the in-app [Help guide](HELPY-SUPPORT.md), offline editing for supported
  Mod Menu 2 controls, and stable-release notifications.
- Refine the world map, keyboard glyph alignment and settings organization.
- Reduce registry and recipe preparation work and refine wearable bridge
  signature searches.

## 2026-11-08 controller and schema follow-up

- Capture controller input in the desktop overlay and wait for held controls to
  release before returning them to the game. Recheck the controller after focus
  returns. Steam and Game Bar can still handle Guide independently.
- Make Settings tabs navigable with the D-pad, take focus directly to the map
  canvas, and improve Tools, Item Bay and Quick Settings controller navigation.
- Keep valid Mod Menu 2 pages visible when another UE4SS mod has a schema error,
  and show that error on its own page. The separate Mod Menu remains exclusive.
- Reset live diagnostics for each attached session. Helpy can exit when its
  attached client or dedicated server closes. Existing preferences are retained.
- Include experimental [Item Delivery](ITEM-DELIVERY.md) for host-configured items
  from enabled mods. The core tracks deliveries by character and waits for a
  matching save acknowledgement. Live grant and save verification are pending.

## 2026-11-08 candidate refresh

- Release nested allocations from temporary reflected map pairs and array
  elements, including partially initialized values and failed imports.
- Use shared sidebar navigation and contextual controller, mouse, and keyboard
  hints. Make cart drops visible and keep compact server actions in reach.
- Add surface atlas area pickers, coordinate overlay, player centering, and
  named Dragon Gate travel to Rasthin's Rest.
- Rotate platform and themed RuneSchema logos on the welcome screen, with
  adjustable timing and optional artwork crossfades.

## 2026-11-07 redirected saves and Helpy follow-up

- Follow redirected character-save folders for copy, import and export. Keep
  nested profile redirects and imports outside the resolved root blocked.
- Share Compact view between RuneSchema and UE4SS, scroll and pan with the
  left stick, and move a selected group to the cart with the right-stick pointer.
- Keep Helpy open when it was started from the desktop. Resolve runtime
  junctions and report bridge errors once per outage.
- Report known older and newer UE4SS versions and use the capabilities
  RuneSchema can verify.

## 2026-11-06 complete refresh

- Controller pointer/click/drag and multi-selection updates.
- Redirected-AppData startup fix on pinned UE4SS; child path guards retained.
- Experimental mod-local templates/providers with resolved-mod ordering and safe
  one-shot tick handoff. Actual cooked typed-array and template storage checks pass.
- All current core, Helpy, bundled plugins and matching Lua adapters repackaged;
  complete package, checksums and source are linked from the candidate page.


Development history follows. Earlier local-testing/publication holds describe
those stages; use the [current candidate notes](PRE-RELEASE.md) for downloads,
installation and remaining limits.

- Keep controller focus inside control borders with a translucent gold highlight, including selectables, item cards and scaled panels. Back cancels or navigates up without hiding Helpy at the root; the Home/Guide shortcut retains overlay visibility control.

- Keep centered popups at their requested width across frames and resizing; preserve caller size limits. Keep physical text input working after controller use by checking the active field’s input source. Let the controller's centered map selector pan in all four directions at overview zoom, with map-edge bounds and corrected hints. Reserve a visible item row in embedded Creation/Recipe bays instead of letting toolbars collapse the results.
- Add mod-owned save declarations through ID.txt or cache/savefiles.json, resolved-path review and verified manual AppData backups. Preserve declarations during cache rebuilds. Mod formats and source saves remain mod-owned.

- Fix Quick Launch and Additional Logging reads failing when their settings replies exceed the small transport buffer. Retry only these read operations with a bounded larger reply; retain single-execution mutation handling. Stop reporting a generic read failure as proof of a mismatched core.

- Simplify Item Bay's search/filter toolbar, consolidate view/cache controls, and add searchable grouped mod sources with reusable provider lists. Use the same picker in inventory, gear and Character Studio; preserve search text and cart contents when clearing filters.
- Refine Mod Menu 2 dials/radials with smooth vector rendering and precise sliders. Add validated RGB/RGBA controls, an explicit template-import action, and a separate runnable external Lua editor example with a portable template. Preserve legacy schemas, callbacks, config formats, authority and stale-target checks.

- Move Mod Menu 2 into **Mods → Mod Menu 2**, beside **Management**. Preserve activation gates, direct shortcuts, editor/controller controls and its License pane. Retain existing navigation ordering while dropping the old main entry. Reuse plugin availability within each refresh instead of repeating its metadata service call. Let the two-pane mod pages own scrolling so controller entry does not stop at an extra outer scroll pane.
- Move the optional Lua ModActor loader's initial dispatch onto UE4SS delayed game-thread actions. Keep load-map hooks active when scheduling is unavailable; avoid an async fallback. Audit notes: [UE4SS maintenance](COMPATIBILITY.md#ue4ss-scheduling-and-api-audit).

- Add Item Bay **Send to player…**: a single-item quantity/player popup, remembered recipient identity, and direct delivery independent of staged cart contents. Keep current-world, connected-player and delivery guards.

- Rework controller navigation: triggers change main sections, bumpers switch contained panels, and the right stick provides a cursor with press-to-click/drag. Add saved cursor controls and clear Always on / Auto detect / Disabled modes. Keep Item Bay quantity adjustment and map pan/zoom accessible without a keyboard. Physical controller and live overlay acceptance remain pending.

- Add registered [multiplayer character appearance](CHARACTER-APPEARANCE.md), server-derived actor/GUID binding, deduplicated committed snapshots, late-join/resync delivery and an optional cooked consumer. The matching RSNetworking trio carries the reliable Actor-reference callback; provider rendering and local profile storage stay mod-owned. Real provider multiplayer acceptance remains pending.

- Add key-gated Mod Menu 2 with Helpy presentation, legacy settings/config callbacks, direct shortcut and optional character-editor tabs/preview transport. Skip its backend beside an enabled or already-loaded external Mod Menu. Preserve Maxxfilth's MIT notice.
- Add a public HUD layout preview for minimap, coordinates, notifications and loading artwork. Save positions without restarting; passive widgets leave input to the game, and interactive Mod Menu 2 uses normal Helpy input capture.

- Prepare saved Item Bay indexes during startup on a background file worker; reuse in-memory category/family buckets for immediate warm filtering. Add portable `cache/taglines.json` rules and optional `cache/tips.json`, shared through cache bundles and preserved across rebuilds.
- Add exact-text ID.txt editing, opt-in local update fingerprints and evidence-based mod-manager labels. Add next-launch UE4SS toggles, per-owner `ue4ss/status.txt`, card/compact views and loading tips. Keep the passive startup indicator independent of the main window mode and retain overlay mode after presentation failures.

- Added Home cache building for enabled mods, plus a persistent per-platform Notifications inbox for mod/game version changes and cache job results. Players offers a confirmed vanilla resync after a reported platform build change. ID.txt versions take precedence over legacy mod.json versions; blank versions do not create update alerts.
- Allowed installed-mod and vanilla PAK cache reading in a separate lower-priority worker while connected. Kept inputs read-only, checked source stability, retained existing cache during updates, and let explicit batches continue/import while Helpy is in the tray. Live lookup remains available for runtime-only data.
- Documented the Wine/Proton investigation and the distinction between a Windows server under Wine and a native Linux hosted server. Linux/Wine runtime verification is still pending.
- Added a separate [Wine compatibility packaging path](COMPATIBILITY.md#0810-wine-compatibility-packaging) with unpacked native DLLs, preserved runtime contents/licenses and a read-only UE4SS export checker. Windows packages retain UPX compression; a real Proton load is still required before claiming the reported `0x45A` failure is fixed.

- Decoupled selected-world cleanup from optional world backups. Disabled world recovery no longer attempts a legacy backup through Game Pass's redirected LocalState folder. Enabled world backups use the AppData archive, and a backup error cannot stop an otherwise validated building repair. Kept the checks that preserve valid records and unrelated world bytes.


This experimental update brings the maintained RuneSchema core, standalone
Helpy app and matching bridge together. Keep the three components from the
same build. The stable release remains separate.

## Local follow-up: Helpy, server access and performance

- Added Item Bay switches for the existing saved cache-first policy and passive updates, plus **Refresh search** for the entered query and current filters. Manual refreshes bypass the automatic cooldown, keep cached rows and use bounded priority requests. Kept offline items, pictures and cart editing available; offline cache misses no longer attempt a game request. Unchanged live checks no longer discard the prepared file index.
- Unified saved native observations and imported references in a reusable search index on the file worker. Removed the older label-only merge and repeated native-page scans for each query; retained source filtering, cache invalidation, and the paged fallback for older caches.
- Smoothed startup artwork and the loading bar at 60 FPS, added buffered desktop painting, and removed the frame limiter's obsolete 15-FPS floor for passive HUDs. Reduced-motion mode stays stationary and startup-stage polling remains once per second.
- Added an optional centered startup display during the black loading screen: alternating Dragonwilds/RuneSchema artwork, an indeterminate loading bar, and current startup stages. Started the desktop companion earlier, retained the normal plugin Unreal-init order, and made completion wait for registration and the menu/world handoff. Startup and coordinate-only HUDs use small passive surfaces, with input left to the game.
- Added an optional **Show player coordinates in game** toggle under Quick Settings → Display. Displays local X/Y/Z and look rotation on one horizontal line at the bottom-right of the game while Helpy is hidden, in Unreal order: Pitch, then Yaw (degrees). Uses a coalesced game-thread sample twice per second, expires unavailable/stale positions, and draws without capturing input. Native presentation supports fullscreen; desktop presentation retains a click-through windowed/borderless fallback.
- Made cache-first searches reuse prepared local search text, shortened their typing delay to 100 ms, and removed immediate game queries on cache hits. Cache misses use the focused live-search route; live discoveries continue saving asynchronously to platform AppData. Passive checks run once per minute while Helpy is open, and explicit source refresh remains available.
- Joined Helpy pane navigation across sections so controllers can leave top tabs and enter Item Bay, settings, tools and other content without confirming empty pane borders. Stopped header focus recovery from interrupting native navigation during scrolling, added direct content entry without automatically activating text fields, and added regressions that follow the real focus path through all sections and the map.
- Added a saved PlayStation touchpad pointer option for PS5/DualSense and DualShock controllers: relative movement, press-to-click/drag, retouch without jumps and click release on focus loss, disconnect or disable. Kept pointer clicks within Helpy's input path.
- Added a saved controller show/hide shortcut: Xbox/PS/Home by default, a View + Menu alternative, or Disabled. Retained the keyboard binding, bounded hidden-state checks to the assigned foreground game and prevented held/rebound/duplicate reports from repeatedly toggling the panel.
- Added an optional Windows GameInput Guide reservation in the game bridge, with direct Guide forwarding, automatic release on Helpy exit and a five-second disconnect lease. Documented that Steam can read controllers independently and still require its own Guide-button configuration.
- Added a saved Resource alerts during startup switch in Quick Settings and Diagnostics. Kept sampling active while suppressing expected startup resource notices for the first two minutes by default, with fresh thresholds and growth baselines afterward.
- Fixed controller-only entry in the standalone window: a detected controller now receives an initial navigation target without a mouse click. Added a PS5 regression that reproduces detection and glyphs working while navigation had no focus.
- Replaced the splash's Enter text with the selected controller's confirm glyph in controller mode. Added controller-only splash, Home tile navigation and tile activation checks at normal, compact and minimum window sizes.
- Mapped left-stick movement to selection navigation with the D-pad, and initialized the selected tab/category when entering a page. Preserved active text fields, drags and popups during focus recovery.

- Moved live catalogue submission, snapshot recovery and native-icon submission off Helpy's UI thread. Bounded pending searches, discarded superseded work and retained connection/search fences for late replies.
- Reused unchanged Item Bay rows and sort order instead of copying the catalogue every frame or rebuilding it for unrelated player/status updates.
- Cleared successfully submitted Item Bay selections, including the legacy primary selection, while retaining items that could not be added to the cart.
- Blocked buffered raw-input events and application mouse commands from reaching the attached game while the interactive HUD owns input. Kept passive notifications non-interactive and preserved cursor/control restoration.

- Added a pinned controller backend for Xbox, DualShock/DualSense and Nintendo layouts, with automatic device labels and manual glyph overrides. Kept the backend out of the game process and suspended it for hidden/unfocused Helpy interfaces.
- Kept Helpy.exe and its helpers in the installed runtime folder. Desktop links target that executable, existing owned AppData links migrate to it, and no executable copies or runtime binding sidecars are created in AppData.
- Added controller-accessible item/mod actions, focused-pane shoulder tab switching, trigger section switching, themed focus highlights and map pan/zoom/center selection.
- Added a collapsible main navigation rail for every input mode, with labeled tooltips, an expand/collapse arrow, a saved pin and restoration of the previous width. Controller mode folds unpinned navigation automatically to give Item Bay more room.
- Added an in-app controller keyboard to Helpy text fields, including nested PIN dialogs, without retaining per-frame user-buffer pointers. Added virtual Xbox/PS4/Switch Pro and password-entry regression checks.
- Guarded popup cleanup when no popup is open and isolated failed server-status reads, clearing stale authority status and delaying retries. Kept notification text inside its card and separated controller map hints from other labels.

- Added stock-UE4SS log-activity and Lua-companion error observations, with configurable message budgets and timing correlation against RuneSchema slow-work reports. These are evidence, not per-mod CPU or polling measurements.
- Added five-second worker samples for Helpy/Dragonwilds CPU, RAM and optional Windows GPU-engine use, with bounded, rate-limited activity diagnostics and unavailable-counter handling.
- Added informational play-time reminders after three observed gameplay hours and then hourly, with a saved Quick Settings switch.
- Added ten diagnostic reference codes covering resource activity, Lua evidence, play-time reminders and server access. New monitor messages append to RuneSchema.log.
- Added notification-only native presentation while Helpy is hidden, with transparent cards and game input left active.
- Preserved server access responses across reconnects and unrelated tool results. Added a read-only join challenge, a dismissible PIN form, a green authority confirmation and host GUID-allowlist controls.
- Verified persisted host PINs, action permissions and GUID allowlists through the settings editor and a simulated restart. Client authentication remains temporary and server-validated.
- Added an Item Bay settings cog with a saved, immediate display/internal-name switch. Switching labels preserves searches, selection and item identity without requesting native data.
- Recovered readable names from the learned catalogue even when published and learned caches already exist, so older imported names do not wait for live rediscovery. Kept prior-version reuse limited to labels for active sources.
- Brought Mods into the shared two-pane layout and added a read-only UE4SS inventory with separate external and RuneSchema-owned groups. Optional Mod Menu metadata supplies versions; unversioned entries remain informational.
- Replaced the open mod list's two-second folder polling with initial/runtime-change reads and explicit refresh, preserving unsaved order drafts.
- Restricted conventional-registry package-name collection to its requested filenames while retaining complete index validation and native fallback for incomplete censuses. Added separate census/probe timings to the existing startup performance log.
- Colored the validated host-mod-list notification by client/host version alignment in both Helpy's panel and desktop popup: green for matching versioned entries, amber for differences and neutral while checking or when no versioned entries can be compared. Unversioned entries and client-only mods do not skew alignment.
- Added side-by-side host/client versions, an annotated Client only tab and a complete plain-text comparison report. Metadata is requested once asynchronously and comparisons are reused until their inputs change.
- Fixed Game Pass crafting wrappers containing indirect parameter helpers while retaining direct-result, type and target-boundary checks. All three full captured wrappers resolve their independently validated implementations.
- Preserved cached display names and native class/type metadata; corrected readable names replace internal keys without changing asset identity. Unresolved observations cannot downgrade readable cached names.
- Used cooked weapon/ammo/armor types when repairing categories, keeping rune weapons out of the Runes filter.
- Kept mass Item Bay selection separate from native detail inspection. Checkbox, Ctrl-click and Select page actions retain selections without inspecting every newly checked item.
- Moved coalesced search cancellation off the UI thread so a cached search cannot wait for the previous live query's cancellation reply.
- Captured double-clicks and polled mouse/keyboard states with the native HUD open, released previously held controls once, and retained normal game-input restoration on hide.

- Added installed-version badges and optional stable author ModID to server mod lists, with distinct missing, disabled, unknown and ambiguous-copy states.
- Added the green RuneSchema-world notification, with a logo and an explicit click action to browse the host's mods.
- Fixed Game Pass crafting thunk bounds using verified contiguous chained unwind records. Fresh Steam native menu checks pass on the same core pin.
- Centered Settings categories and applied the two-pane layout to Tools and local Server Settings. Added a return action for mod-to-cache navigation and compact cache input/progress stacking.
- Added sequential **Cache enabled mods**, with pause/resume/stop and per-mod results. Previous caches are copied and verified under AppData before the mod's current cache is replaced.
- Corrected Home's footer coordinate conversion while scrolling and reserved more space for runtime and social buttons at compact sizes.
- Included culture-invariant cooked FText when exporting item names and descriptions. The Dye fixture now exports all 2,241 names without `ITEM_` fallbacks; old caches need one rebuild for the original display labels.
- Kept native panels within shared-texture bounds on oversized viewports, retained the open request during resize, and preserved the usable surface when invalid replacement dimensions are rejected.
- Suppressed the game's synthesized Slate hover while the overlay captures input, without changing Helpy's own pointer or another process's cursor coordinates.

- Added a bounded host-owned snapshot of enabled RuneSchema mod IDs, names and versions, including JSON-only and script-only mods. Joining players receive it through the existing reliable bridge; clients cannot replace the authority list.
- Added **Helpy → Mods → Server mods**, with search, clipped rows and a clickable received-list notification while Helpy is open. Leaving a world clears the previous host's list.
- Included optional download-page links from each mod's ID.txt website metadata. Helpy opens them only on click, validates HTTP/HTTPS links and keeps mods without a link visible.
- Enriched the game's existing modded-world warning when it is still open and its native fields are available. Its scrollable body can display the full list; unsupported layouts retain a short preview and the complete Helpy view. Original warning text and button actions remain game-owned.
- Added [RS-NET-004 and RS-NET-005](../ERROR-CODES.md#rs-net-004) to the shared diagnostic reference. The mod list is informational and does not grant PIN access or bypass registry checks.
- Fixed native crafting and building-menu material checks for Builder's Dream and Artisan policies, including alternative-material recipes. Requirements and AnyOf flags are restored after each scoped native call; unlock, quantity, station and authority checks remain active.
- Refreshed an already-open material menu when its policy changes, retaining the selected recipe and quantity. Building-slot refresh is bounded across frames; closed menus stay closed and no extra global polling was added.

This follow-up is a local candidate. Eight isolated native UI checks passed on
Steam, including crafting an output and switching policies without reselection.
Authenticated client/server gameplay and rendered native-warning acceptance
remain to be checked; no production installation or upload is implied.
See [server mod lists](SERVER-MOD-LIST.md) and
[building policies](BUILDING-WORLD-POLICIES.md).

## Local follow-up: world recovery opt-in

- Reworked health, stamina and carry-capacity profile rules to own native Add modifiers, preserving equipment and cooked world effects during profile refresh. Capture their baselines from native defaults and difficulty scales, retain unchanged handles, and retire previous-pawn state on possession changes. No extra polling or global effect enumeration was added.
- Avoided missing-package warnings from optional conventional registry names when a complete mounted-container census proves the package is absent. Retained native fallback for incomplete indexes and validation of actual declarations.
- Refreshed native Well Rested before resolving its tracked instance, so a refresh can safely track an instance replaced by the game. Native need maxima remain unchanged.
- Fixed uppercase platform labels such as `GAMEPASS` being rejected by save-backup routing and reported as an invalid world.
- Moved indexed world snapshots and world restoration behind **Settings → Helpy → Saves → World save recovery (experimental)**, disabled by default for each runtime.
- Kept character recovery and existing verified backups before cleanup independent of the world-recovery experiment. Optional world archive failures no longer prevent cleanup after a rollback backup verifies.
- Retained overlay keyboard, text and mouse-button events while rendering holds its lock, including Backspace and button releases. Events wait in a bounded queue instead of being silently discarded.
- Added game-thread movement/look suppression for an open Helpy overlay. Closing it or disconnecting the desktop bridge releases only Helpy's own input blocks; regular window mode remains separate. Added diagnostic `RS-UI-003` for an unavailable native input contract.
- Restored the game's latest cursor constraint after hiding the native overlay, with conditional focus/capture recovery for the foreground game. Alt+Tab does not restore a game constraint over another application.

## Helpy menu and interaction refinements

- Added a [Save Recovery](SAVE-RECOVERY.md) navigation page for whole-character and whole-world snapshots, with optional snapshots at selected character/world entry.
- Recorded backup reasons and the active mods' IDs, names and versions beside save archives. Mod files are never copied or restored; players reacquire them themselves.
- Added verified offline restoration for matching existing Steam/Epic characters and Steam/Epic/server worlds, plus one-shot native-entry restore requests for Game Pass.
- Preserved the current save before replacement and stopped damaged, mismatched, changed or redirected restore inputs from replacing it.
- Added visible Item Bay selection checks, Ctrl-click and a controller-friendly selection mode, plus a bottom **Send to cart** action. Drag/drop and context actions remain available.
- Retained selected rows across search/page changes, with a 64-item cap and resets on runtime changes.
- Coalesced repeated cached/live results by asset path, including case-only path aliases, while preserving different assets that share a display name. Newer source information updates the existing row.
- Positioned item details beside the pointer with viewport bounds and retained popup widths; kept cards tall enough to scroll instead of compressing their contents.
- Saved normal window positions and sizes after move/resize events, plus floating panel layouts, without adding DPI polling.
- Moved read-only connection-status requests off the UI thread, limited them to one request at a time, and discarded replies from an old runtime.
- Deferred initial Item Bay discovery while Helpy is hidden in the tray; opening it still restores its cache and requests live content normally.
- Added RS-SAVE-003 through RS-SAVE-005 to the shared diagnostic reference without changing existing save-code meanings.
- Kept normal world cleanup at the selected world-load boundary, with folder sweeps reserved for the explicit Blanket fallback.
- Scoped Game Pass's expected modded-building checks to IDs in that world's saved building list. Verified enabled definitions remain protected before menu application, avoiding the unrelated `Dye/dye_station` readiness warning seen in a world containing other modded equipment.

- Kept Settings' category column and divided the right pane into short tabs with independent scrolling.
- Applied the same tab treatment to Quick Settings, Utilities, Spawns, Journal, Diagnostics, authoring workflows and the cache builder.
- Standardized preference switches; retained checkboxes for selections and action confirmations, and radio buttons for exclusive choices.
- Increased panel contrast so themed artwork does not compete with text and controls.
- Kept save/launch reads out of unrelated Quick Settings tabs and removed unnecessary per-frame Utilities menu allocations.
- Removed the fixed 40 ms delay on isolated bridge updates while retaining the existing 25-per-second limit during notification bursts.
- Added RS-OVL-001/002/003 for visible presentation, failed attachment and bridge readiness, with website guidance. Overlay messages now use RuneSchema.log without duplicating them in UE4SS.log; other plugin messages retain their existing routing.
- Left RuneSchema's minimal UE4SS menu unchanged.

## Startup and the clean package

- Reuse mounted-directory checks across assets in the same folder during startup discovery.
- Reuse registry-folder ownership checks and a bounded, temporary weak-object snapshot during declaration validation. A stale or renamed object still falls back to native lookup and loading; lane validation remains atomic.
- Share unchanged cooked manifests internally instead of copying their JSON on every publication. Validate merged declarations without constructing discarded recipe bodies; normal callers still receive independent owned definitions and snapshots. Conflicting batches remain all-or-nothing.
- Removed a duplicate reflected-frame validation from the blocking asset-load wrapper; the remaining count check still validates the complete live parameter frame before invocation.
- Added scoped startup timings separating wrapper overhead from time inside Unreal's reflected loading call, including its ProcessEvent callbacks. These clocks are inactive outside the selected startup batches.
- Queue recipe references before other assets in the existing native prefetch hint. Retained the 512-request bound after a local 4096-request experiment showed no benefit. Required assets still pass normal live loading and registration checks.
- Check live, fully ready recipe objects before asking the blocking loader to resolve a reference missing from the startup snapshot. Missing, stale and wrong-class objects still use the native loader and normal validation.
- Split discovery timing into manifest validation, registry query, metadata scanning and candidate loading in RuneSchema.log; recipe timing reports snapshot hits, live hits and native load requests.
- Compress the core and Helpy bridge DLLs; test compressed files and compare decompressed code, exports, resources, relocation/unwind data and imported APIs before packaging. UPX may rewrite PE metadata.
- Build the clean ZIP from committed plugin content and default configuration, with canonical PAK triplets and dependency notices. Exclude mods, profiles, logs, saves, generated mappings and cache data.

Existing local traces identified discovery and recipe loading as startup costs on
Steam and Game Pass. The earlier Steam build logged 44.34 seconds of engine
initialization (54.4 seconds from the Steam launch request to front-end
initialization), 5921 ms of declaration processing and 12,295 ms of recipe
preparation. The optimized candidate logged 35.72 seconds, 1850 ms and 10,440 ms
respectively with a larger prefetch queue. A subsequent run at the original
512-request limit logged 27.16 seconds, 1649 ms and 5238 ms, with matching
registry counts and no RuneSchema warnings or errors. Both reached the front end
and closed normally without entering a world.

The final rebuilt 512-request candidate reached the front end in 28.11 seconds
from the Steam launch request, with 24.54 seconds of engine initialization,
1626 ms of declaration processing and 5199 ms of recipe preparation. It also
closed normally; no character or world was loaded. This candidate is installed
on Steam for local acceptance testing, with publication held.

These are individual runs, not a cold-start statistical benchmark: warm files
and other active applications affect the result. The larger prefetch queue was
removed; the lower-copy declaration processing is retained. Game Pass has not
yet been timed with this follow-up. UE4SS and the game's own loading remain
separate costs.

An offline 24,000-recipe publication benchmark, repeated three times with matching
output checksums and atomic conflict rejection, reduced median publication time
from 2489 ms to 316 ms. This measures declaration processing, not total game
startup or native asset loading. A separate Steam profile measured 12,129 ms
inside the recipe load calls and 160 ms in their wrapper, identifying the native
loading path as the main remaining recipe cost.

## Settings and the game panel

- Made native overlay the default for new settings; existing window preferences
  remain intact. The regular desktop window remains available as an option.
- Fixed rejected 10-bit SDR game buffers preventing the Helpy shortcut from
  displaying the native panel. DX11/DX12 target views avoid applying sRGB
  encoding twice, preserving the normal theme brightness.
- Added back-buffer format and first-frame evidence to bounded overlay logs.
  Native-opening failures also show their reason in a usable desktop window.
- Added a native hardware cursor, rendered fallback and current-position pointer sampling. Mouse
  movement no longer fills the input queue or competes with frame composition;
  clicks keep their original positions, including scaled render viewports.
- Fixed DX12 overlay frames being omitted when a back-buffer allocator was busy.
  A bounded submission pool reuses completed allocators without GPU waits on
  the presentation thread.
- Fixed visible composition being omitted on resource-lock contention and
  forced complete DXGI `Present1` updates while captured, preventing stale
  panel/cursor pixels from game-only dirty rectangles.
- Replaced the Settings tab strip with a category panel on the left and a
  separately scrolling settings panel on the right.
- Added direct access to Helpy startup, shortcuts, performance and cache
  preferences from the main Settings page.
- Added **Settings > Helpy > Game overlay**, also in Quick settings. The complete
  themed interface stays in Helpy's process and uses the existing bridge.
- Added adjustable overlay opacity. Opening the panel centers it within the
  attached game's client area. Hiding retains that display surface; disabling
  overlay mode restores the desktop window bounds.
- Added consistent switches across Settings and Quick settings, with dependent
  overlay and startup choices disabled when unavailable.
- Made the navigation logo return to the welcome splash without clearing drafts.
- Added native DX11, DX12, OpenGL and Vulkan shared-GPU compositors. Helpy
  remains a separate process; native opening keeps the game foreground.
- Verified DX12's presentation queue from back-buffer transitions, including
  enhanced barriers. Resize hooks release old buffers before the game resizes.
- Added opt-in Vulkan startup enrollment for each runtime, with a removable
  per-user selector and no changes to the game's renderer selection.
- Added GPU fence ownership, bounded input queues and heartbeat expiry. A stalled
  or disconnected app releases captured mouse, keyboard and controller input.
- Changed the desktop presenter to DirectComposition/flip-model where available;
  retained regular windows, system tray access and the legacy rendering fallback.
- Added overlay size and interface scaling. Native mode keeps detached components
  inside the shared surface; tray activation opens the normal desktop interface.
- Added Automatic/Mouse & keyboard/Controller navigation, selectable Xbox,
  PlayStation and generic glyphs, section shortcuts and a controller footer.
- Made custom item, cart, search, recipe and inventory controls navigable.
- Added a first-launch desktop shortcut to each runtime's installed Helpy app,
  labelled with its platform; launching it refreshes the AppData executable.
- Fixed the native-overlay request buffer so preparation, attachment and visibility
  actions reach the game bridge instead of falsely reporting it unavailable.
- Preserved an open request during the first game attachment and allowed the
  hotkey to hide native presentation after the desktop window was minimized.
- Matched Helpy's UI device to the game's reported GPU on hybrid systems,
  preserving CPU caches and rebuilding GPU resources on an adapter change.
- Kept overlay text and controls opaque while adjusting background transparency.
- Hid Home tile labels when compact tiles cannot fit them; tooltips retain names.
- Added bounded overlay readiness/attachment entries to RuneSchema.log and kept
  native status visible outside the optional Vulkan setup section.
- Kept Game Pass desktop shortcuts outside package activation while explicitly
  assigning their runtime; other platform shortcuts use their installed app.
- Added isolated GPU tests for all four renderers, Vulkan startup/delegation,
  DX11 render-state restoration, DX12 bridge lifetime, shared-buffer ownership,
  controller mode and shortcut preservation. Live exclusive-fullscreen and
  multi-monitor/driver acceptance remains outstanding.

## Authoring, reports and startup

- Added Tools > RuneSchema repair: one Repair button, progress and a console.
  It reads recent log hints, verifies the matching GitHub package, checks bundled
  base files, backs up replacements and rolls back failed writes. User mods,
  settings, saves, caches and third-party plugins are preserved.
- Added Tools > Authoring tools with Installed/Creation cache-builder tabs,
  progress controls, private .NET setup and ZIP/7z conversion.
- Added Create/Update Cache to the Mods context menu. Creation preserves loader
  folders, writes ID.txt/cache and produces a Binaries/Content ZIP in Downloads.
- Preserved native UE4SS DLL companions under their normal loader directory;
  RS-CACHE-015 explains the integration checks their author needs to make.
- Changed bug reporting to a plain-text clipboard report and Nexus bugs page.
- Added a bounded ready-menu fallback for Quick Start, native save selection
  checks, an arming-based deadline and foreground-only Shift cancellation.

## Registry messages and safeguards

- Routine building-ID exchange and nonblocking registry-parity messages remain
  available in Diagnostics without interrupting gameplay by default.
- Added **Show routine registry synchronization notices** in notification settings.
- Retained operation-specific placement rejection, protocol checks, identity
  validation and all error reporting. A quieter notification display grants no
  permission to use mismatched native network IDs.

## Client controls for an opted-in server

- Added **Settings > Server access** for connecting with a host-provided PIN.
- Added private host settings for PIN access and client self-teleportation;
  all new permissions default to disabled.
- PIN sessions are bound to the native RPC owner, current character and world.
  Changing worlds, changing the PIN or restarting the host revokes access.
- Limited failed authentication attempts, bounded stored sessions and expired
  idle access after an hour. The client does not save the entered PIN.
- Allowed authenticated self-delivery and self-teleport requests through the
  existing RuneSchema authority bridge. The server executes each operation.
- Preserved server item/quantity checks and native teleport ground checks.
- Required matching item-ID audits or a verified authority-order proof before
  remote delivery, independently of unrelated client-only registry lanes.
  PIN access cannot target another player, spawn actors, place buildings,
  edit server configuration or authorize registry mismatches.
- Distinguished queue acceptance from operation completion. Delivery results
  return through bounded, correlated receipt parts to the requesting client.
- Rejected stale-world or wrong-component responses and timed out missing
  results without automatically replaying the mutation. Inspect the inventory
  or player position before retrying an unconfirmed operation.

See [server access setup](HELPY-STANDALONE.md#server-access-with-a-world-pin).
Authenticated production client/server delivery and travel still require a
live acceptance pass; the feature is opt-in in this experimental build.

## Maintained refinements included since 0.8.0.3

- Included the standalone app/bridge separation, tray lifetime controls,
  runtime-specific cached browsing and missing-mod-entry cleanup.
- Included bounded live item/icon discovery, source refresh, update-aware
  caches, corrected family/source grouping and cart recipient handling.
- Included the Game Pass native farming/support binding fixes and independent
  failure handling for optional building-policy adapters.
- Included generic opt-in public dismantling, phasing and scoped building and
  crafting material callbacks. Native outputs, progression and station checks
  retain their own contracts; dedicated crafting coverage remains partial.
- Included cooked-registry/table compatibility work, Vortex packaging support,
  authoring-cache tooling and the corresponding author documentation.

The experimental branch contains the source and versioned build configuration.
It does not update an installed game, retag a stable release or replace stable
download assets by itself.

## Local controller and editor development

These changes are local candidates and are not in the published download.

- Contain controller focus within active sections. Confirm enters; Back returns to the parent. Bumpers switch the nearest tabs and triggers switch workspaces.
- Add stationary one-finger tap-to-click for DualSense and DualShock 4, preserving swipe movement, physical click/drag and two-finger right-click.
- Add the isolated `"--Helpy"` schema namespace, circular numeric controls, radial choices and reviewed import/export of mod-declared settings templates.
- Report invalid optional plugin DLLs through Helpy instead of leaving a Windows loader dialog blocking startup.

## Local Advanced Tooling refinement

These changes are unpublished local candidates.

- Present the optional developer plugin as **Tools → Advanced Tooling**, with
  themed capability cards, shorter notices and a Compatibility report tab.
- Coalesce player/nameplate file changes by the containing folder within a
  queued reload batch. Those loaders already reload the whole folder; other
  loaders retain separate file jobs. This targets auto-reload work during play,
  not the game's measured boot time.
