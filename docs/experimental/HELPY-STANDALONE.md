---
channel: experimental
promote_to: HELPY-STANDALONE.md
---

!!! info "Folder layout during testing"
    The [complete 2026-11-06 candidate](PRE-RELEASE.md) uses the [new runtime layout](RUNTIME-LAYOUT.md): Helpy under `Helpy`, core plugins under `dlls/plugins` and Helpy plugins under `Helpy/plugins`. Legacy layouts remain supported.


!!! info "Pre-release · 0.8.1.0"
    See [candidate status](PRE-RELEASE.md) for downloads and remaining live checks.

# Helpy desktop app

For screenshots and troubleshooting, open the [Helpy support guide](HELPY-SUPPORT.md).

Helpy runs as `Helpy.exe` from
`RuneSchema/Helpy/`, beside its bridge and `SDL3.dll`.
AppData holds settings, caches, profiles and logs. Keep all components from one
matching runtime package.

## Welcome splash and map

In **Settings → Backgrounds → Welcome splash**, choose which logos rotate,
set seconds per logo, or turn off the artwork crossfade. The RuneSchema logo
uses your theme accent. Backgrounds crossfade when optional artwork is
available. Windows reduced motion holds a static splash.

The footer follows the active input mode. Item Bay shows mouse buttons,
keyboard shortcuts, or controller glyphs. Drag one card or a selected group
onto the cart or the **Drop into cart** target above the results.

In **World → Map**, click a world map zone for its close-up, or choose a zone from the picker. **Transport outlines** toggles the zone and portal guides. **Scorned Wilderness** uses a separate coordinate view. **Find player** opens the player's area, and **Fit map** resets the current view.


## Cache updates and notifications

| Action | Where |
| --- | --- |
| Build caches for enabled mods sequentially | **Home → Build enabled mod caches**, or **Mods → Cache enabled mods** |
| Pause, resume or stop a build | The Tools progress view |
| Review cache changes and job results | **Notifications**, below Report a bug |
| Resync vanilla files after a game update | The **i Game cache update** action on Items |
| Switch display/internal item names immediately | Item Bay's settings cog |

The read-only PAK builder works with the assigned game open or closed. Existing
cached items and pictures remain available while it builds; completed results
are imported into platform AppData. Disabled or removed sources are excluded.
ID.txt versions and local tracking fingerprints can prompt cache updates.

Cached searches run separately from the interface. New live results are learned
for later searches. A readable name replaces an internal key; unresolved metadata
does not downgrade it. A cache without display text needs corrected metadata.

Use checks, **Ctrl+click**, or **Select multiple** with a controller, then
**Send to cart**. Adding items clears that selection. Drag/drop and right-click
remain available. Different asset paths may legitimately share a display name.

## Mods and UE4SS inventory

Mods, Settings, Tools and Server Settings use category/content panes with
independent scrolling. In Mods, choose **RuneSchema mods**, **Server comparison**
or **UE4SS mods**. Apply staged order changes for the next launch.

UE4SS entries use cards; **Compact** switches to a table. **Manage external UE4SS
mods** opts into editing `mods.txt` and backed-up `enabled.txt` overrides.
**RuneSchema loaders** changes only a mod's nested `ue4ss/status.txt`: `0` disables
that loader, `1` or no file enables it. The parent's PAK/JSON content is separate.
These actions do not unload running scripts.

Versions come from optional ID.txt, mod.json or Mod Menu metadata. Missing
versions are labelled **Unversioned** and excluded from host-alignment totals.
Configured state and observed loader health are separate. See
[metadata and controls](MOD-METADATA-AND-CONTROLS.md).

### Selected mod licenses (in development)

Select a RuneSchema or UE4SS mod card to give it a gold border. Open **License**
at the bottom of the internal navigation to read its bundled notices. Compact
UE4SS rows can also be selected. Selection does not enable a mod or change its order.

Helpy reads common `LICENSE`/`LICENCE`, `COPYING`, `NOTICE`, terms and conditions
files, including `conditions.license` and `license-jonesing4space.txt`. It also
checks the mod's immediate `licenses`, `licences` and `legal` folders. Each notice
is shown separately and can be copied; **Refresh license** reloads local changes.
Files load in the background, without a connected game or a network request.

**Open mod page** uses the author link from `ID.txt` or supported metadata. If a
local notice is missing, check the permissions or license on that page. Helpy
does not scrape Nexus Mods/CurseForge or infer a license from a download link.


## Startup and the system tray

1. Install the complete package and launch Dragonwilds. Helpy normally starts
   in the tray; use **Open window at startup** to show it immediately.
2. Press **F2** while the assigned game has focus, or open the RuneSchema tray icon.
3. Use **Home → Quick settings** or **Settings → Helpy** to change preferences.

Display, Startup, Cache, Performance and Saves tabs share the same controls in
both locations. Changes apply immediately unless marked for the next launch.
The top-left RuneSchema logo returns to the splash page. The navigation bar can
collapse or remain pinned.

**Close to system tray** makes X/Alt+F4 hide the window. Use **Exit Helpy** to quit.
The tray tooltip identifies the runtime. Windows controls whether its icon is
visible or under the overflow arrow. If the tray is unavailable, Helpy uses the
taskbar instead.

**Exit game-launched Helpy when the game closes** follows the game or server
process that started Helpy. Helpy opened from its desktop shortcut stays
available after the game closes. A temporary bridge disconnect keeps either
session open.
Leftover-process reports are still under investigation; see [known limits](PRE-RELEASE.md#known-limits).

The executable stays in its installed runtime folder. The desktop shortcut
points there and identifies the installation. Game auto-launch uses Explorer's
desktop environment instead of inheriting a Steam/Xbox session. AppData is not
an executable installation directory.

## Use it with the game closed

Open the runtime's `Helpy.exe` or its desktop shortcut. Saved names, pictures,
filters and carts work offline. Settings, next-launch mod order, imports and
cache tools remain available. Use **Mods → Refresh** after changing files elsewhere.

Missing-folder rows can be removed from the load-order list without deleting
installed folders. Item delivery, teleportation, uncached native discovery and
live character previews require a connected game. Cached data does not install
missing mods or establish that an item can be spawned.

## Player coordinates in game

Enable **Quick Settings → Display → Game HUD → Show player coordinates in game**.
The horizontal readout shows **X/Y/Z**, then **Pitch/Yaw** in Unreal's order.
It defaults to the bottom-right, uses game world units and updates twice per second.
Missing/stale character data hides it. It is passive and takes no game input.

Use [HUD settings](MOD-MENU-2-AND-HUD.md) to position supported HUDs. Minimap
zoom, player-follow lock and position lock are separate saved controls.

## Startup loading indicator

**Show game startup indicator** displays the alternating Dragonwilds/RuneSchema
logos, native spinning wheel, startup stage and optional mod tips. It needs a
usable game window and bridge. It follows startup checkpoints, not the intro
video's end; the unsuccessful post-intro timing change is excluded.

The indicator closes when registration and menu/world handoff are ready.
It shows activity without inventing a percentage, respects reduced motion and
takes no input. Its saved HUD placement is retained; startup data is read once
per second independently of animation. It also works when Helpy uses a regular
standalone window.

## Use Helpy over the game

Enable **Settings → Helpy → Display → Window & overlay → Game overlay** and
**Native in-game presentation**. F2 opens/hides the panel in the assigned game;
**Hide** returns input. Mouse, keyboard and controller input are captured while
an interactive HUD is open.

Native presentation supports exclusive fullscreen. Its supported DX11, DX12,
OpenGL and Vulkan paths have separate readiness checks. Adjust panel size,
interface scale and background opacity in Display. Resize keeps the overlay
request; it does not permanently switch to a desktop window.

Vulkan requires this runtime's **Vulkan startup adapter** enrollment and a game
restart. It does not change the game's graphics API. Desktop presentation is an
option for windowed/borderless games; disable Game overlay for a regular window.
See [native overlay setup and limits](NATIVE-OVERLAY.md).

### Controllers and offline access

Xbox, PlayStation and Nintendo navigation/glyphs support menus, Item Bay, map
controls and text entry. A manual glyph override and alternate shortcut are
available. Driver/connection differences and Windows/Steam Guide handling still
need hardware testing. See [controller controls](CONTROLLER-NAVIGATION.md).

## Quick Start readiness

Choose **Quick Launch**, select the native character/world and **Save launch
options**, then restart. Hold Shift in the game to use normal startup.
Standalone QuickLoad conflicts, ambiguous saves and unsupported contracts leave
the normal menu available. Status in Quick settings explains why automatic entry
is waiting. Quick Start is separate from Save Bypass.

## Server access with a world PIN

A server's local Helpy or its owner's client can save player authorization,
authorize all connected players, or enable automatic authorization. Saved grants
record the server-observed character, account name and platform. Native Admin
is a separate tab and permission.

Alternatively, a host can enable PIN access and its permitted item/teleport
actions. A client joins the world, opens **Settings → Server access**, submits
the PIN and waits for **Client-server authority bridged**. The server validates
and executes requests for that character. Quantity, identity and ground checks
remain enforced; an accepted request is not a confirmed delivery.

Keep host settings private. PIN sessions are temporary; saved player grants
persist independently. See [access setup](SERVER-ACCESS.md) and
[server controls](SERVER-CONTROL.md) for revocation, persistence and role details.

## Registry notification preferences

Routine registry synchronization is quiet by default but remains in history/logs.
Enable **Show routine registry synchronization notices** under Diagnostics for
toasts. Warnings and blocked operations retain their own notification behavior.

Cleanup notices use plain language: **Removed X records from uninstalled mod: X**,
or **Removed X records** when ownership is unknown. Notifications supports clearing
all entries. Matching-version hotfix releases can be reviewed and applied with
the game closed; see [notifications and hotfixes](NOTIFICATIONS-AND-HOTFIXES.md).

## Optional Game Pass game log

Under **Diagnostics → Evidence → Options → Game Pass game log**, save a logging
shortcut or copy the launch command. Restart through that shortcut to request
`-log` and `-ABSLOG`. Shipping builds may still suppress native logging.

The default target is the runtime's AppData diagnostics folder. Game and EOS
streams read it when available. **Keep logs in UE4SS (next launch)** changes
RuneSchema/Helpy log locations; recreate older game logging shortcuts afterward.
The toggle does not change other launchers' command lines.

## Direct delivery and player marks

Right-click **Give** can be used with a populated cart. It waits for an existing
background validation request, then submits the direct selection once without
consuming or changing the cart. A queued request is canceled if its recipient
disconnects or the world changes. An unconfirmed delivery is never retried
automatically; check the recipient's inventory before submitting again.

Character marks stay saved locally while an owned-player networking bridge is
unavailable. RuneSchema warns once and retries with a delay up to 30 seconds;
it reports recovery when forwarding resumes. Host and client need compatible
RuneSchema networking components for synchronization. A request that was sent
but not acknowledged still reports a separate failure. Mark synchronization
does not require an administrative item-delivery PIN.

## Responsiveness and background work

The default UI limit is **60 FPS**, adjustable from **15–144**. Hidden windows do
not render; unfocused windows use a lower rate. Windows' animation preference
controls transitions. Cache reads, searches, notifications and game requests use
bounded background work rather than blocking input.

Caches and profiles stay in per-runtime AppData. By default, Helpy's diagnostic
folder keeps a rotating 2 MiB desktop log and one previous file. Overlay status
changes use RuneSchema.log: [RS-OVL-001/002/003](../ERROR-CODES.md#rs-ovl-002).

## Repair RuneSchema

Close the assigned game/server. In Helpy, open
**Tools → Utilities → Repair** and press **Repair**. The console checks required
base files against the exact installed version's verified GitHub release ZIP.
Missing/changed base files are replaced; user mods, settings, saves, caches and
UE4SS are outside its scope.

Repair keeps previous files and a receipt in the AppData tool job folder.
Failed writes roll back; any rollback problem includes its backup path. Locked
files or unavailable/unverified packages leave the installation unchanged.

## Validation boundary

Use matching core, bridge, application and plugin files. The UI and cached
metadata do not bypass server authority or native gameplay checks. Test results
and remaining platform/hardware limits are listed in [candidate status](PRE-RELEASE.md).
