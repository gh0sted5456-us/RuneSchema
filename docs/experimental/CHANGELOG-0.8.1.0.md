---
channel: experimental
promote_to: CHANGELOG-0.8.1.0.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# RuneSchema 0.8.1.0 — experimental

This experimental update brings the maintained RuneSchema core, standalone
Helpy app and matching bridge together. Keep the three components from the
same build. The stable release remains separate.

## Helpy menu and interaction refinements

- Kept Settings' category column and divided the right pane into short tabs with independent scrolling.
- Applied the same tab treatment to Quick Settings, Utilities, Spawns, Journal, Diagnostics, authoring workflows and the cache builder.
- Standardized preference switches; retained checkboxes for selections and action confirmations, and radio buttons for exclusive choices.
- Increased panel contrast so themed artwork does not compete with text and controls.
- Kept save/launch reads out of unrelated Quick Settings tabs and removed unnecessary per-frame Utilities menu allocations.
- Removed the fixed 40 ms delay on isolated bridge updates while retaining the existing 25-per-second limit during notification bursts.
- Added RS-OVL-001/002/003 for visible presentation, failed attachment and bridge readiness, with website guidance. Overlay messages now use RuneSchema.log without duplicating them in UE4SS.log; other plugin messages retain their existing routing.
- Left RuneSchema's minimal UE4SS menu unchanged.

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
