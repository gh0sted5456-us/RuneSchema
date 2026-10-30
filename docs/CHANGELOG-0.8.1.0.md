# RuneSchema 0.8.1.0 — experimental

This experimental update brings the maintained RuneSchema core, standalone
Helpy app and matching bridge together. Keep the three components from the
same build. The stable release remains separate.

## Settings and the game panel

- Replaced the Settings tab strip with a category panel on the left and a
  separately scrolling settings panel on the right.
- Added direct access to Helpy startup, shortcuts, performance and cache
  preferences from the main Settings page.
- Added **Settings > Helpy > Open Helpy as a game overlay**. The complete
  themed interface stays in Helpy's process and uses the existing bridge.
- Added adjustable overlay opacity. Opening the panel centers it within the
  attached game's client area; closing it restores the desktop window bounds.
- Overlay placement checks run at most four times per second while visible.
  Hiding it stops rendering. Switching to another app removes its topmost state.
- Kept the normal Helpy shortcut, system tray, navigation, detached components,
  caches and desktop mode. No graphics injection or new engine polling was added.
- The overlay targets windowed/borderless games. Exclusive fullscreen and
  every monitor/DPI combination still need live acceptance testing.

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
