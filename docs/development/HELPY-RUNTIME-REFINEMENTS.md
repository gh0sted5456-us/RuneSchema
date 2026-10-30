# Helpy runtime refinements

Development candidate, October 29, 2026. These changes address identified code
paths; the reported crash on opening Helpy has no matching runtime/log/dump, so
its cause is not established.

## Window and menus

The main renderer now unbinds its back buffer before resize, checks resize and
presentation failures, and stops drawing when its device/context/swap chain or
target is missing. A failure emits `RS-UI-001` once per distinct reason.
Preview checks exercise a missing surface before any ImGui frame is started.
This does not make arbitrary graphics-driver failures recoverable.

Quick Settings remains directly below the Home tiles, in a centered scrollable
panel. Cache order, launch mode and cleanup activation timing use circular
radio buttons; independent on/off preferences stay checkboxes. Home's Quick
Settings and Profile buttons have matching heights.

Submitting a search can synchronously clear its projected matches after the
frame's first catalog rebuild. That transition now shows a loading state until
the new projection is ready, instead of flashing a final empty result. Cached
hits and native misses keep their existing bounded cache-first/search paths.

An obsolete, unbuilt Helpy plugin source was removed. The active Helpy plugin,
native bridge and supported host ABI remain in their established locations.

## Work while closed or minimized

| Work | Policy |
| --- | --- |
| UI rendering, player refresh | Only while visible and not minimized. |
| Automatic catalog discovery and source census/publication | Pause while hidden; resume when opened. |
| Theme-file checks | Pause while hidden. |
| Log-file reads | Visible UI, or explicitly enabled diagnostic toasts while the game is focused. |
| Hotkeys | Existing lightweight OS key checks continue so the UI can reopen. |
| In-game FPS overlay/notifications | Continue when enabled by the user. |
| Requested cache builds, catalog jobs, delivery, travel and gameplay hooks | Retain their existing lifecycle and budgets; closing Helpy does not cancel an accepted action. |

Visibility is published to the core only when it changes. The core defaults to
hidden and resets that state at shutdown. A bridge size probe cannot alter it.
No claim is made that closing Helpy stops RuneSchema's gameplay responsibilities
or every enabled overlay. Tests check scheduling gates; production FPS and
frame-time improvements still need measurements on representative mod sets.

## Registry ordering

The pre-join candidate now includes buildings alongside the registrar's
reorderable lanes. It validates all shared identities/types/native prefixes,
plans the server's entries first, and retains client-only entries afterward.
Building order is staged before travel and applied when constructing the next
world registry, never rewritten on the previous live world.

Building compatibility can compare a verified shared prefix even when a client
has additional local definitions. The hash is calculated from the actual world
registry, not inferred from a successful handshake. IDs outside the peer's
range and different identities remain blocked for placement.

The compact welcome upgrades individual lanes to exact authority asset paths
when their compressed representation fits. A full SHA256 proof of the selected
paths, identities and native types must match before planning. Extra entries in
the same mod source can then remain local. Large lanes retain source/delta
compression and still require identical content within each authority source;
otherwise they warn and alignment is skipped. The decoder retains support for
the previous delta format. The candidate extends its welcome limit from 48 to
60 KiB and its expanded-document limit from 2 to 8 MiB. Owner/source metadata
supports 1,024 entries. Parsing retains a depth limit and now also has a total
element budget. Oversized offers are reported instead of truncating IDs.

Gameplay identities take priority over optional source/version telemetry. If a
compact offer still exceeds the limit, optional telemetry is reduced and a
notice identifies that reduction. Every shared identity remains in the plan;
the order proof covers the full lane mappings and fixed-lane fingerprints,
independently of the optional metadata. No gameplay IDs are silently dropped.

An isolated native localhost test transmitted a 59,506-character welcome and
synchronized seven lanes. The actual 884-entry authority building prefix
matched the client's 889-entry registry, including each native index; five
client-only entries remained afterward. This engine reported a 262,144-byte
partial-bunch limit and a 16,777,216-byte network-string limit. This establishes
the tested native transport, not authenticated EOS gameplay certification.
Larger transport still requires a verified
paged pre-join protocol; changing a number does not establish that protocol.
The native uint16 index capacity for each registry remains unchanged.

The native welcome bootstrap remains **opt-in** through
`RUNESCHEMA_NETWORK_ID_SYNC=1` pending complete real multiplayer certification.
Its executable-specific checks and native-ID preservation remain in place.
The normal compatibility bridge continues to work without this opt-in.

| Registry shape | Boundary |
| --- | --- |
| Items, recipes, quests, combat/utility spells, held effects | Existing startup forward/reverse registry transaction. |
| Supported additional Dominion data families | Same transaction when their startup adapter verifies and enrolls them. |
| Buildings | New pre-join planning and deferred world-registration adapter. |
| Melee/ranged attack class collections | Ordered per-component arrays/CDOs, not interchangeable global data-subsystem IDs. Existing compatibility fingerprints remain required; generic reindexing is not enabled. |
| Skills/perks | [Investigation and remaining lifecycle requirements](../SKILL-REGISTRY.md). |

Clients still require the server's cooked assets. Shared IDs do not synchronize
different gameplay rules, transfer PAKs or make client-only content usable by a
server that lacks it. Independent new players negotiate their own lists; an
existing joined player's IDs are not rewritten to admit another client.

## Optional external desktop and installer follow-up

The [standalone desktop candidate](../HELPY-STANDALONE.md) reuses the existing
GUI DLL in a separate executable host, with local runtime-specific JSON IPC.
The EXE is now the sole UI host; the DLL contains bridge services only. Offline settings/order/import operations
reuse existing filesystem logic; game operations retain their native adapters.
The complete local runtime includes the executable; an independent app archive remains available.

[Vortex packaging](../VORTEX-INSTALLATION.md) adds explicit Steam/Game Pass
install mappings and an explanatory note in the ordinary PAK mods directory.
Case-insensitive container groups, ignored metadata-only directories and
per-plugin failure isolation address identified startup failure paths. The
reporter's exact deployment fault remains unconfirmed without filenames.

## Mounted item cache regression

The inspected Game Pass manifest contained 39,264 rows, including Adamant
Catapult Contract. Its sidecar contained 39,177 older rows. A 2,063,633-byte
serialized roster exceeded the old sidecar scope limit, so the full manifest
updated while the search index stayed stale. Scope identity now hashes the
roster into a compact digest (112 bytes in this case). Readers reject a sidecar
older than its manifest and recover through the current saved rows. Legacy
scope comparisons migrate without discarding compatible browsing data.

Typed/family searches with cached hits also schedule bounded mounted checks.
Mounted ItemData metadata is checked before the large loaded-object roster;
new rows merge without replacing the visible cache. Provider refresh and
same-mod family lookup use this path and persist discoveries through the
existing learned-item worker. A source refresh preserves old data on failure.

Tests cover the large roster, stale/replaced sidecars, cooldown, bounded
continuation, offline/changed-session handling, and mod-family filtering.
The native contract was confirmed in the PAK directory metadata and in the
saved manifest. These probes did not mutate PAKs, character saves or worlds.


## Primary app and notification-area follow-up

The renderer is statically linked into Helpy.exe. The in-game bridge target
links no ImGui, D3D11, DXGI or WIC renderer. Startup can go directly into the
system tray, with show/exit actions and Explorer restart recovery. The retired
DLL-host selector is removed. Bridge wake notifications, bounded FIFO drains,
read fast paths and buffer reuse reduce avoidable IPC work. UI frame pacing is
60 FPS by default with a 144 FPS ceiling; data refresh remains independently
throttled and hidden rendering stops.
