# Helpy jitter investigation — October 27, 2026

Dedicated-server logs show 1,721 expired movement messages in the latest reviewed
session and 9,879 in its predecessor. These are evidence of movement desync, not
proof of an exhausted RuneSchema relay. The connection is LAN-addressed and
advertises netspeed 100000. Neither session logs a reliable-buffer overflow.

The previous session also has at least 4,611 unsupported Black Trebuchet component
replication warnings. The latest movement failures occur without that warning
storm, so it is not sufficient to explain every rubber-band. Findings were
forwarded to the separately maintained Siege Warfare task.

## Measured Helpy work

The server's `helpy.icons` phase reports roughly 146–157 ms peaks once a minute.
Its source census reports 213,189 exact package associations. One `helpy.tools`
window reports a 406 ms peak and about 1.2 seconds of work across ten seconds.
The existing broad phases do not identify every operation responsible.

An unchanged source refresh previously replaced large immutable ownership maps
and could release their old trees on the game/UI thread. The implementation now
compares discovered maps against the published indexes on the existing discovery
worker and retains equal indexes. Changed owners, additions and removals still
publish new indexes; validation and content fingerprints are unchanged.

A local synthetic benchmark with 213,189 package associations measured the old
main-thread publication at **117.1568 ms**, compared with **0.0014 ms** when equal
indexes were retained. Comparison and duplicate cleanup took **237.945 ms on the
worker**. These are isolated publication timings, not an in-game FPS comparison
or proof that all network jitter is fixed.

Additional aggregate phases distinguish source publishing, foreground catalogue
queries, full indexing, passive refresh, map snapshots and map markers. They use
the existing bounded timing meters and reporting cadence, not per-frame log I/O.

## Completed verification

Both shared DLLs build as 0.8.0.2. Twelve focused core tests and thirteen Helpy
tests pass, including registry contracts, worker cleanup, update hints, transport,
map imports and three UI sizes. The asset-download fixture also passes twice in
succession. Strict documentation, snippets and local links pass. Both compressed
DLLs pass UPX integrity checks and the Helpy plugin-boundary check.

The combined core and Helpy DLLs were installed on Steam, Game Pass and the
dedicated server after normal closure. All six installed hashes match the
staged files; previous DLLs have verified local backups and an installation
receipt in ignored local storage.

## Remaining verification

- Compare the same live client/server workload before and after installation.
  Confirm periodic source publication no longer creates the recorded spikes.
- Use the new phases to locate the remaining long tool operation rather than
  assuming it is a search, map request, or icon capture.
- Before raising bandwidth caps, measure per-connection bytes, queued bits,
  packet loss and latency. Bulk mod data must retain byte budgets and leave
  capacity for movement and gameplay.
- Investigate the separate persistent-store failures reported after a client
  timeout. Successful world saves do not establish successful player-store saves.

No firewall or server bandwidth configuration was changed by this investigation.
Raw log copies stay in ignored local investigation storage;
they contain account identifiers and should not be published.

## FPS controls and idle pacing follow-up (2026-10-28)

The optional caption counter now displays engine tick FPS and Helpy rendering FPS
separately. Server desktops label the first value as server tick rate. An O(1)
counter reuses the existing engine-tick callback and publishes two cached atomics
once a second. The UI reads them at the selected 0.5–2 second cadence only while
the counter is enabled and rendered. This does not add UObject enumeration,
network traffic, disk access, an engine hook or a thread.

Helpy's focused render limit is adjustable from 15–60 FPS (default 30). Unfocused
rendering remains 10 FPS and hidden/minimized windows render nothing. The window
loop now waits up to 16 ms between visible checks and 50 ms while not renderable,
instead of waking every 8 ms; input messages still wake it immediately. Its wait
also respects the next render deadline and does not sleep past an overrun.

The remaining tool spikes are not established as fixed. Existing slow-work
reporting now attributes active travel, map ground queries, player role refresh,
map snapshot requests, player snapshot requests and other requests separately.
These scopes run when the corresponding work occurs and retain the existing
10-second slow-work reporting cadence. Nested scopes overlap; do not sum them.
