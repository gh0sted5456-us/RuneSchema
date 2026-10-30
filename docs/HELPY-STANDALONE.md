# Helpy desktop app

Helpy's interface now lives in **Helpy.exe**, in its own Windows process.
`RuneSchema.Helpy.dll` stays in Dragonwilds as the small game bridge. It does
not create the renderer, menus or application window. RuneSchema's core runs
the validated game operations behind that bridge.

This is part of the 0.8.1.0 experimental build. Keep its matching core, bridge
and executable together; the complete runtime package includes all three.
An independent app archive is also available for replacing a matching EXE.

## Startup and the system tray

Settings uses two panels: choose a category on the left and edit its controls
on the right. **Helpy** contains startup, shortcuts, performance and cache
preferences; **RuneSchema** contains the core configuration editor. Your
existing settings files and saved preferences remain in use.

1. Install `main.dll`, `RuneSchema.Helpy.dll` and `Helpy.exe` from the same build.
   The packaged app belongs beside the bridge in
   `RuneSchema/plugins/RuneSchema.Helpy/dll/`. The running copy is staged into
   `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/apps/<installation>/`.
2. When the plugin is enabled, **Start Helpy with Dragonwilds** defaults to on.
   The default **Start in system tray** mode opens no application window.
3. Press the configured Helpy shortcut while its game has focus, or click the
   RuneSchema icon in the Windows notification area beside the clock.
4. In **Home > Quick settings**, select **Open window at startup** to show the
   app immediately instead. Startup preferences apply next launch.

Windows controls which notification icons appear directly beside the clock
and which appear under its overflow arrow. The app respects that choice.
Right-click the tray icon for **Open Helpy** or **Exit Helpy**. Its tooltip
identifies Steam, Game Pass, Epic or Server. Explorer restarting restores the
icon without restarting the app. These controls use the
[Windows notification-area API](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area).

**Close to system tray** is enabled by default. X or Alt+F4 then hides the
window while keeping its process available; the minimize button also returns
to the tray. Uncheck it to make X exit. **Exit Helpy** in Quick Settings or the
tray always quits. If Windows cannot create the tray icon, Helpy retains a
minimized taskbar window so it remains reachable.

Plugin settings are stored in `settings/settings.jsonc`:

| Setting | Default | Effect |
| --- | --- | --- |
| `enabled` | `true` | Enable the game's bridge/plugin. |
| `startWithGame` | `true` | Launch the app with its game. |
| `openOnGameStart` | `false` | Show a window instead of starting in the tray. |
| `minimizeOnClose` | `true` | Return X/Alt+F4 to the tray instead of exiting. |

The retired `standaloneDesktop` label is ignored; there is one UI host.
Disabling the game plugin does not prevent manually browsing the app offline.
Headless servers retain bridge functionality without attempting to show a UI
on a non-interactive Windows desktop.

## Use it with the game closed

Double-click `Helpy.exe` to open its assigned installation. Browse saved item
names/pictures, manage profiles and caches, edit settings, change next-launch
mod order and import mod ZIPs. Existing validation, expected-content checks and
backups apply. Enabled installed add-ons determine offline mod-cache visibility.
After changing add-ons elsewhere, use **Mods > Refresh** to reconcile it. Right-click a missing-folder row for **Remove missing entry**, or use **Remove all missing entries**. These stage load-order list changes; choose **Apply for next launch** to save them. Installed folders are retained, and a folder that reappears before applying requires a refresh.

Uncached native discovery, live appearance rendering, item delivery and
teleporting need the connected game and retain their native validation.
A saved reference never proves gameplay availability. The bridge does not
transfer cooked assets or make an absent mod usable.

## Use Helpy over the game

Open **Settings > Helpy**, enable **Open Helpy as a game overlay**, and choose
the opacity. Press your normal Helpy shortcut in Dragonwilds to open or hide
the themed panel. All normal Helpy pages remain available. Disabling the option
returns the window to desktop mode.

The panel runs in `Helpy.exe`; it uses the existing game bridge for gameplay
actions. It follows the attached game's client area while visible, checks its
position at most four times per second, and stops rendering when hidden. It
does not attach a graphics hook to Dragonwilds. It releases its topmost state
when you switch to another application.

Use windowed or borderless mode. An exclusive fullscreen game can cover desktop
overlays. Live fullscreen/monitor/DPI acceptance testing remains necessary.

## Server access with a world PIN

This is an **opt-in experimental** alternative to the existing host allowlists.
Install matching RuneSchema, Registry Bridge and networking components on the
host and client. Keep the actual gameplay content installed on both sides.

For the host:

1. Open **Settings > Server access > Host configuration (this installation)**.
2. Enable **Allow pin access** and enter an 8–64 character PIN without spaces.
   Prefer a long, unique code and share it only with intended players.
3. Enable **Allow client item grants**, **Allow client teleport**, or both.
   Set the maximum item count per request.
4. Choose **Save host settings**, then restart the host/server normally.

The equivalent settings in the host's `settings/settings.ini` are:

```ini
[helpyAuthority]
allowPinAccess = true
accessPin = "replace-with-your-private-world-code"
allowClientItemGrants = true
allowClientTeleport = true
maximumItemCount = 100
```

The PIN is stored in the host's private configuration file, not encrypted by
RuneSchema. Do not distribute that configured file or include it in a public
mod ZIP/support report. PIN authentication uses the existing game connection;
this feature does not add a separate encrypted transport.

For a player:

1. Join the world with your character.
2. Open **Settings > Server access**, enter the host's PIN, and select
   **Connect to server**. Check the server's response.
3. Use the ordinary item cart for your own character, or the map's teleport
   action for a selected position. The server validates and executes the request.
4. Select **Disconnect** to revoke this access. Reconnect after changing
   character/world, restarting the host, rotating its PIN or an hour of inactivity.

The client clears the entered PIN after submission and does not persist it.
Item delivery also requires verified item network IDs for that connection:
either matching audited item lists or a verified authority-order proof. An
unrelated extra registry lane does not reject delivery. Teleport permission
does not relax the separate building-placement identity guard.
Five failed attempts temporarily block that peer for one minute. A shared
failed-attempt budget also protects reconnects; disconnecting does not clear
failed authentication attempts. Sessions are
bounded and tied to native character/world identities. A PIN does not authorize
other-player targeting, building placement, actor spawning or server settings.

**Accepted** means the server queued the command. Helpy waits for a correlated
result before calling a delivery confirmed. A missing result times out after
60 seconds without retrying the operation. If it is unconfirmed, inspect the
inventory or current position before making another request.

Remote delivery/teleportation still needs live authenticated production
acceptance testing. Keep it disabled on a public server until you have checked
your matching build and intended content in a test world.

## Registry notification preferences

Under **Settings > Diagnostics**, routine registry activity is quiet by default.
Handshake progress, completion and nonblocking content differences remain in
the diagnostic history and logs. Enable **Show routine registry synchronization
notices** to also display them as toasts. Actual blocked placements, unrelated
warnings and errors remain eligible for notifications; no validation is bypassed.

For an advanced shortcut:

```text
Helpy.exe --runtime-root "D:\Game\RSDragonwilds\Binaries\WinGDK\ue4ss\Mods\RuneSchema" --platform gamepass
```

Both game launches and opening the packaged `Helpy.exe` ask Windows Explorer
to launch that AppData copy. Helpy therefore
runs outside the game's Windows package and inherits the desktop environment instead of
keeping the Xbox or Steam game session alive. Its `runtime.json` binds it to
the original installation; a new game session reconnects to the existing app.
Copying and launch work happen once on a startup worker, with no recurring
file polling. Exit Helpy before replacing a running app with a newer build.

Platforms: `steam`, `gamepass`, `epic`, `server`. Game auto-launch supplies its
platform; specify it for ambiguous custom paths. Each installation gets one app
instance and its own local bridge, so Steam and Game Pass can run separately.

## Optional Game Pass game log

Open **Diagnostics > Evidence > Options**, then find **Game Pass game log**.
Hover **(?)** for the instructions and current limits. **Save logging shortcut…**
creates a Windows shortcut to your assigned Game Pass installation with
`-log` and `-ABSLOG` arguments. Close Dragonwilds normally, then use that
shortcut for the next launch. **Copy launch command** provides the same
arguments as a PowerShell command.

The intended output is
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/gamepass/diagnostics/RSDragonwilds.log`.
Helpy automatically reads that file in the Game and EOS streams when it exists,
keeping it separate from a simultaneous Steam session. Existing native logs
remain available as a fallback. Saving the shortcut does not change Xbox's
normal launch settings, restart the current game or enable verbose polling.

The shortcut requests native logging; it does not guarantee that a game build
will produce a file. Shipping builds can disable logging at build time.
See [Epic's argument reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-command-line-arguments-reference)
and [Microsoft's GDK launch guide](https://learn.microsoft.com/en-us/gaming/gdk/docs/tools/tools-pc/launching-on-pc).

## Responsiveness and background work

**Diagnostics > Evidence > Options > Log folder** includes **Keep logs in UE4SS
(next launch)**. This saves `logsInUE4SS` in that installation's `settings.ini`.
Restart both Dragonwilds and Helpy to write `RuneSchema.log`, `RSDiagLog.jsonl`
and `Helpy-Desktop.log` beside `UE4SS.log`; RuneSchema session archives use
`ue4ss/logs/`. Existing logs stay where they are. Disabling the option restores
the usual locations on the next launch. No background copying or extra polling
is added. Cache storage remains in platform AppData.

Game Pass logging shortcuts created after changing the option use the selected
folder for `RSDragonwilds.log`. Recreate older shortcuts. Native logs from other
launchers require `-ABSLOG="<absolute ue4ss path>/RSDragonwilds.log"`; the toggle
does not change those launchers' command lines. EOS shares the native game log.

The pipe is local to the same Windows user, rejects remote clients and validates
the server PID/install directory. Messages contain bounded JSON, not native
pointers. The app waits on an update notification instead of issuing an empty
read every 200 ms. It coalesces activity for 40 ms and drains bounded FIFO batches
(up to 16 events and roughly 1 MiB, allowing one larger validated event).
Small health checks and a fallback for older bridges remain. Waiting never
holds the request lock, so explicit item/teleport requests can proceed.

Read-only calls try a real reply buffer first, avoiding the routine size-probe
round trip. Mutations reserve their required buffer and are never replayed after
an unknown outcome. Response buffers are reused. Reconnection clears stale
live state while cached browsing remains available.

Visible cached pictures are read together on the file worker and installed in
small, time-bounded UI batches without waiting for native discovery. Already
resident pictures are reused immediately. Missing pictures use at most four
queued bridge requests; the game still captures one uncached icon at a time
within its existing work budget. Successful requests do not impose a 100 ms
pause before queuing the next icon. Newly captured pictures stay in the runtime
cache for later searches and launches.

Cached searches prioritize matching name prefixes, then wrap through the
remaining records so alternate names are still considered. Family matching
also recognizes native armour identifiers such as `Armour T5 Head Splitbark`.
Existing cached family rows stay browsable while bounded live checks continue.
If a saved catalogue has an outdated or missing disk index, its first fallback
read rebuilds that index on the file worker for later searches and launches.
The original catalogue and source ownership checks are preserved.

The UI defaults to **60 FPS**, adjustable from **15 to 144** in Quick Settings.
Existing explicit limits remain respected. Unfocused windows use 10 FPS; tray
windows do not render. The application's presentation pacing is independent of
game refresh requests and the game's own frame limit.

Page bodies ease into view over 160 ms, with the background and navigation
staying in place. Home tile exits take 200 ms; navigation remains responsive.
Sidebar rows have rounded hover feedback and a clear active-page marker.
Windows' **Animation effects** preference disables these transitions; Helpy
reads that preference at startup and when Windows reports a settings change.
It does not poll the setting each frame.

The frame limiter keeps a steady deadline after a slightly late frame instead
of adding that delay to every later frame. Focused windows use a
[high-resolution waitable timer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)
when available, with the existing message wait as a fallback. Waiting yields
the UI thread and wakes for input; it does not busy-wait or call `timeBeginPeriod`.
Hidden windows keep their existing no-render behavior.

The application, taskbar, Alt-Tab and system tray use the RuneSchema logo,
embedded at ten sizes from 16 to 256 pixels. It is available offline and needs
no downloaded artwork. Windows identifies the executable as **RuneSchema Helpy**.

Item Cart retains the latest delivery outcomes for reordering, without keeping
unrelated catalogue or snapshot fields from acknowledgments. Completed request
queues remain bounded and consumed acknowledgments are removed. Renderer
shutdown drains pending icon reads and frees item, map, preview and wiki texture
records before destroying their ImGui context. Cached files stay available for
the next launch.

Quick Settings includes **Exit Helpy when the game closes**, saved as
`"exitWithGame": true` in the Helpy plugin's `settings/settings.jsonc`.
It is off by default and applies immediately. When enabled, Helpy exits fully
after its attached game or dedicated server process ends, even if closing the
window normally sends it to the tray. Temporary bridge disconnects do not
close it. Starting Helpy offline still works; the setting follows a process
only after attachment. Windows' process-exit event shares the existing desktop
wait, without adding game queries or a polling worker.

A completed imported-cache search with no matches now falls through to the
bounded mounted-item search. An older base-game cache cannot hide a newer mod
item simply because its imported index finished first. Results found live are
queued for the existing learned cache; ordinary cache hits remain available
immediately.

By default the app keeps its own rotating 2 MiB log at
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/diagnostics/Helpy-Desktop.log`.
It retains one previous log and never writes into the game's log. Runtime
Details can show the installed Vortex icon as a mod manager, not a runtime.

## Validation boundary

The migration requires matching application and runtime components. Native
item/map gameplay and representative game-FPS/server-jitter measurements remain
separate release checks. GUI services from third-party plugins are not a general
RPC API; the built-in tools and native inventory-mark service retain their
specific adapters. Desktop `Version` and registry JSON `SchemaVersion` labels
are informational; shapes, owners, budgets, storage formats and ABI contracts
still receive validation.
