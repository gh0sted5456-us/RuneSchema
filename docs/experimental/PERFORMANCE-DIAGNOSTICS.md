---
channel: experimental
promote_to: PERFORMANCE-DIAGNOSTICS.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental build. See [stable downloads](../CURRENT-RELEASE.md)
    for the public package. These observations help investigate a problem; they do
    not certify that a mod is healthy or identify the cause of a crash.

# Resource, UE4SS and play-time diagnostics

Open **Settings → Diagnostics → Performance** for measurements and thresholds.
**Quick Settings → Performance** also has switches for resource diagnostics,
UE4SS activity diagnostics and play-time reminders. These are saved preferences;
changes apply without restarting the game.

**Resource alerts during startup** is off by default in both menus. Measurements
continue, but CPU, RAM, GPU and private-memory-growth notices wait two minutes
after monitoring starts or a new game process attaches. Turn it on to include
startup activity. Changing this switch applies immediately; it does not change
other diagnostics or the five-second sampling rate. Existing preferences without
this setting use the quiet startup behavior.

## Computer and process measurements

One worker samples every five seconds while Dragonwilds is attached or Helpy is
visible. The table separates the computer, Helpy and the attached Dragonwilds
process. No process injection, process suspension or game-object scan is used.
Turning resource diagnostics off stops further resource sampling. GPU sampling
has its own switch, and missing or warming counters show as **Unavailable**.

CPU is elapsed process CPU time divided by elapsed time and the machine's logical
processor count. It is a share of total CPU-time capacity and may differ from
frequency-adjusted Task Manager numbers. See Microsoft's
[GetProcessTimes contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes).
Process RAM includes working set and private memory when Windows permits the
read. Computer RAM is physical memory use; other applications can account for
pressure that Helpy and Dragonwilds do not explain.

GPU readings use Windows GPU-engine counters. Each process reports its busiest
sampled engine; the computer row combines process activity per engine and shows
the busiest engine. An engine at full utilization can be normal during gameplay.
These measurements do not require the game and Helpy to use the same renderer.
Driver/counter availability still matters. See Microsoft's
[GPU performance overview](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/)
and [wildcard counter API](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw).

CPU/RAM/GPU activity notices require three high samples. The default thresholds
are 90% CPU-time capacity, 90% physical RAM use and 98% GPU-engine activity;
adjust them for expected workloads. Repeat notices have a ten-minute cooldown.
After the startup quiet period, three fresh high samples are required; loading
activity is not replayed as a delayed warning. A normal sampling pause resets
measurement baselines without starting another startup quiet period.
A separate informational observation reports private-memory growth exceeding
256 MiB over a minute. Loading and retained caches can explain that growth:
compare steady gameplay, page changes and item delivery before calling it a leak.
Sampling gaps and process identity changes reset the relevant baselines.

## Stock UE4SS evidence

RuneSchema keeps **unmodified UE4SS**. It reads the current runtime's UE4SS log
and identifies messages only when a known mod name or an unambiguous metadata
label appears in a supported bracket/quoted Lua-mod message. An unknown or
ambiguous name is left unattributed.

**Mods → UE4SS mods → Observed health** shows observed log activity or a reported
Lua companion error. Prior log errors can annotate the list without replaying
them as new warnings. The original timestamp and UE4SS message remain the evidence;
a configured mod or an observed message is not proof that every feature works.
RuneSchema-owned native/hybrid UE4SS packages retain their unsupported annotation.

The default activity budget is 100 attributed messages per read window, sampled
about every five seconds. It measures **log output**, not callback executions,
polling frequency or CPU time. Raising the budget can accommodate a chatty mod.
If a burst occurs near a measured RuneSchema slow-work report, an additional
notice identifies the timing overlap. The default overlap window is five seconds
and is adjustable from one to thirty seconds. Correlation does not establish
contention or identify which mod caused a delay. No hooks are blocked or reordered.

Readers, mod attribution and notice queues have bounds. Notices are grouped and
rate-limited; original UE4SS messages remain in their source log. New monitor
diagnostics append to **RuneSchema.log**, respecting the saved log-directory
setting. They do not write additional messages back into UE4SS.log.

## Notifications in fullscreen

With **Game overlay → Native in-game presentation** enabled and its matching
bridge ready, notices also render over the game while Helpy's main panel is
hidden, including supported exclusive-fullscreen renderers. Only notification
cards are drawn on a transparent surface at up to 15 frames per second while
notices are visible. This does not draw Helpy's background, poll Item Bay,
capture the cursor, block game controls or take focus.

Open Helpy with its normal shortcut to interact with notices or inspect history.
Passive notices cannot be clicked while game input is active. In-game visibility
still depends on a working native renderer; an older bridge without passive
presentation retains desktop notification behavior.

## Play-time reminders

After three observed gameplay hours, Helpy shows this informational notice:

> You have been playing for three hours, consider taking a 10 minute break.

It then reminds you at each additional complete hour. The timer counts observed
gameplay while the game has focus, using cached connection/world observations.
Menus, disconnected intervals and long suspend gaps are excluded. Dedicated
server operation does not count as a player's gameplay session. Reminders never
pause the game, capture input, take focus, disconnect you or change saves.

Turn off **Quick Settings → Performance → Play-time reminders** to stop them.
The observed timer resets when Helpy or the attached game restarts; it is not
an account-wide or historical play-time tracker.

## Reference codes

- [RS-PERF-002: process CPU activity](../ERROR-CODES.md#rs-perf-002)
- [RS-PERF-003: physical memory pressure](../ERROR-CODES.md#rs-perf-003)
- [RS-PERF-004: GPU-engine activity](../ERROR-CODES.md#rs-perf-004)
- [RS-PERF-005: private-memory growth](../ERROR-CODES.md#rs-perf-005)
- [RS-LUA-001: Lua companion error](../ERROR-CODES.md#rs-lua-001)
- [RS-LUA-002: attributed log activity](../ERROR-CODES.md#rs-lua-002)
- [RS-LUA-003: slow-work timing overlap](../ERROR-CODES.md#rs-lua-003)
- [RS-PLAY-001: break reminder](../ERROR-CODES.md#rs-play-001)
