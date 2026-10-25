# Troubleshoot RuneSchema and PAK mods

Work from the first failed step instead of changing several files at once. A
PAK can mount while an item inside it still fails to load, and an item can load
while its recipe still points to the wrong station.

## Start here

| Symptom | First check |
| --- | --- |
| RuneSchema never starts | Active UE4SS installation, `Mods/mods.txt`, then the first startup failure |
| Helpy stays closed | F2, enabled Helpy plugin, then Home → Quick settings → Open Helpy when the game starts |
| Item is listed but its picture is blank | Native icon/cache status; do not delete the item mod or save |
| PAK mounts but content is absent | Exact cooked path, native enrollment and its recipe/build/station connection |
| A patch changes nothing | Correct loader, case-sensitive field/directive, target class/row and first rejected field |
| Save rejects entry after changing mods | Back up the original, inspect failed mounts and cleanup messages; retain the last working mod set |

Open **Diagnostics** for grouped issues and its **Live Log** view. Start with the
first failure from the same launch, rather than a later symptom.

## Heavy Blueprint/Lua editors and building registry compatibility

The RSHC author reported editor slowdown alongside RuneSchema and supplied a
source-level explanation. Bridge ProcessEvent callbacks now reject unrelated
calls by cached function names before reflection, path allocation or route
searches. Relevant bridge/graph classes are matched through serial-validated
weak class caches after exact path verification. RPC and authority validation
are unchanged. This removes identified overhead; a live RSHC before/after timing
comparison is still required to establish the actual speedup.

This overhead does not depend on where the other mod is installed. Moving an
unmanaged mod under RuneSchema may improve its loading/ownership diagnostics,
but is not a fix for editor hook overhead. Never move or duplicate a live mod
automatically; back up, close the game/server, and follow its author's supported
packaging instructions if a diagnosed loading conflict warrants migration.

Building network indexes must agree on server and clients. Do not change
persistence IDs, re-sort/renumber indexes after world load, or adopt an ID-prefix
workaround without a separately verified offline save migration. Cached package
ownership cannot prove building-index parity or authorize a save conversion.
The supplied RSHC migration scripts and process-start proxy were not adopted.
Large client/server appearance exchange is separate from RuneSchema's bounded,
allow-listed presentation cues; this change adds no unrestricted data relay.

## Helpy frame-time refinement (pre-EXE)

Helpy remains a plugin DLL with its own UI thread/device. Its desktop redraw is
capped at roughly 30 Hz while focused and 10 Hz while unfocused; hidden and
minimized windows do not render. Detached surfaces omit unrelated draw lists.
Favorites/Junk idle polling is capped at roughly 30 Hz; button edges, queued
marking input and inventory hover notifications force a fresh check. Stack
ownership and item identity are still revalidated before changing a mark.

Catalogue queries, full-scan work and icons share one optional work slot per
engine tick, with a 2 ms start deadline. Icons and catalogue alternate priority;
mutations retain their own mailbox and take precedence. This is a soft budget:
a native asset load or GPU readback cannot be interrupted after it starts.
Thumbnail file writes run on a single bounded filesystem task; the game polls
completion without waiting and publishes success only after the file commits.
The existing same-call GPU draw/readback is retained for Game Pass compatibility.

`[PERFORMANCE][SUMMARY]` in RuneSchema.log reports aggregate calls, total/average/peak microseconds
and calls at or above 2 ms, at most once per ten seconds per measured call site.
Cheap phases stay silent; summaries bypass debug-only logging but do not flood
the in-game console. A first measured stall of a second or more still warns
immediately. These are RuneSchema phase timings, not game FPS or
proof that a particular feature caused a frame drop. Compare the same scene,
game focus and settings with Helpy hidden, Home visible, uncached icons visible,
and inventory open; separate cold-cache work from steady-state measurements.

## RuneSchema console commands

In the game console, run `rs help` (or `runeschema help`). The console must
already be enabled; the stock UE4SS ConsoleEnablerMod normally binds F10 and
tilde. RuneSchema does not replace the console or install a remote shell.
Stock UE4SS's `HookLocalPlayerExec` must remain enabled. These commands use
that hook; `rs help` lists them even if `DumpConsoleCommands` does not.

| Command | Result |
| --- | --- |
| `rs status` | Runtime, game/framework versions and host compatibility. |
| `rs network` | Observed world/network role and freshness, not a multiplayer round-trip test. |
| `rs mappings` | Active mapping identity/index; does not force regeneration. |
| `rs services` | Loaded plugin services, capabilities and connections. |
| `rs vision` | Helpy's cached native styling status and bounded diagnostic samples. |
| `rs menu` / `rs exitmenu` | Request the local player's native pause/exit menu. Close the console to see it. |

Diagnostics do not change saves, settings, gear or layouts. `rs menu` is the
only UI action: it does not click Exit, disconnect, travel, or quit. It requires
a local, possessed world player and matching native menu reflection contracts;
dedicated servers have no local-player menu. A request or selected Pause screen
is not proof that the screen is visibly open. Unavailable contracts are reported
without guessing offsets or invoking an alternate function.

`rs vision` requires Helpy and its native component. `Active=true` is not a
health certification: check viewport dimensions, Errors, Unsupported, Deferred,
Scanning and Complete. Its samples prioritize errors and group repeated reasons;
they are not the full inventory tree. The command never starts a scan or applies
a layout. Command replies also appear under `[CONSOLE]` in RuneSchema.log.

Automatic styling preserves minimap/world-map image and border brushes,
including RuneUI's live map artwork. Unclassified controls retain their native
appearance; the unsafe spell Cost subtree remains deferred rather than traversed.

## Helpy Diagnostics, Live Log and help codes {#helpy-live-log-and-help-codes}

The [diagnostic code reference](ERROR-CODES.md) explains each stable code,
its effect and the next step. Older uncoded messages can show a **suggested
reference**, not a diagnosed cause. Consecutive repeats can be grouped without
dropping the underlying entries from a filtered export.

Open **Diagnostics → Live Log** in Helpy. The **RuneSchema** source reads
`RuneSchema/RuneSchema.log`; the **UE4SS** tab reads the host's `ue4ss/UE4SS.log`.
Each tab retains its own search, filters, selection and scroll position.
The **Game** tab discovers a recent Dragonwilds log under the game's saved-log
folders. **EOS** shows EOS SDK categories from that game log, not a fabricated
independent stream. Either tab can choose an existing log through **Actions**.
The exact file path appears on hover. Shipping builds may not create game logs;
Helpy reports this rather than enabling logging or altering launch arguments.
Search text or an `RS-` code, filter by severity
or subsystem, pause reading, follow the newest entries, and select a row for
its full message. **Clear view** clears only the viewer, never the log file.
The recent history is bounded to 4,000 entries / 2 MiB of text; first opening
reads the recent tail, not an unlimited archive. Each tab has its own limit.
Log rotation starts a fresh view for the affected file only.

Expand **Filters & colors > Live Log colors** here, or **Settings > Appearance**, to color entries
by keyword or source/subsystem. Rules are literal and case-insensitive, not
regular expressions. Keyword matches override source matches; the first rule
of each kind wins. Colors persist in `ui.jsonc`; they do not rewrite either log.

**Actions > Export filtered entries** saves the currently retained matching entries, with
their help codes, to a local UTF-8 file. It is not an export of an entire
previous session or both tabs combined. Open the selected log's folder for
the original and its rotation backups. Neither live log can be overwritten
by a filtered export.
Review player names, IDs and local paths before sharing an export.

To open Helpy once at game startup, enable **Home > Quick settings > Open
Helpy when the game starts**. It takes effect next launch. The current package
enables this by default; an existing installation retains its own saved choice.
F2 still closes/reopens the window. It never enters a world.

For slow item discovery, sharing caches or changed mods, see
[cache bundles and background verification](CACHE-BUNDLES.md).

Clickable help codes open a per-code header on the shared
[diagnostic code reference](ERROR-CODES.md), which lists every registered RS
code and bridge reason with causes and troubleshooting. No log text, player
identity or file path is sent in the link. An exact emitted code is distinct
from a **suggested reference** for an older uncoded message; the original log
and severity remain authoritative. The older sections below remain available
for existing bookmarks, with additional context rather than a second code catalogue.

### RS-QST-001 — Conflicting quest ownership {#rs-qst-001}

One saved quest identity has competing owner stamps, or more than one enabled
mod declares it. Preserve the complete message, including the persistence ID
and source filenames. If a quest moved between mods, enable only the intended
current declaration and load with the matching updated RuneSchema build: the
verified live ownership handoff preserves progress and normalizes old stamps.
A genuine simultaneous declaration conflict must be resolved by the authors.
Do not delete quest progress or character files to hide the error.

### RS-REG-001 — Registry enrollment deferred or rejected {#rs-reg-001}

Read the complete rejection reason. Deferred certification means RuneSchema
has not proved the whole registry safe; it is not proof every asset failed.
The affected registry kind can be skipped when authority is unavailable;
independently verified sections can still be cleaned after a verified backup.
Incomplete registry checks do not suspend every cleanup lane. Exact pending
declarations and uncertain records remain protected.
Check required mounts, duplicate identities, registration capacity and the
first failed enrollment message. Fix the source or installation before using
an important save. Never force certification or manually remove save entries.

### RS-NET-001 — Client/server registry mismatch {#rs-net-001}

Compare the RuneSchema version, enabled mod list, PAK versions and registry
fingerprints on both sides. Fully restart after aligning them. Matching mod
folder names alone does not establish matching content. Preserve logs from
the same connection attempt on both sides. An authentication success does not
establish registry parity or prove the server reached a playable world.

### RS-WEAR-001 — Wearable contract not enrolled {#rs-wear-001}

An item exists but its native wearable/hide contract was not confirmed.
Compare its asset path, slot, mesh/material references and contract declaration
against the intended item. Re-equip only after the correct package and
contract load. A visible item can still have the wrong appearance. Do not
mark an unverified bridge as enrolled simply to suppress the warning.

### RS-PERF-001 — Slow runtime work {#rs-perf-001}

The measured phase exceeded its work budget; this message alone is not a
network rejection. Record the phase, source, elapsed time and whether the
spike repeats. Native asset load or actor construction can be indivisible
even when surrounding discovery is sliced across frames. Turn on advanced
logging temporarily for detailed attribution in `RuneSchema.log`, then turn
it off after collecting the evidence. Report the slow phase rather than
removing the performance check.

### RS-CAT-001 — Item catalogue unavailable or expired {#rs-cat-001}

Retry the catalogue and reset restrictive category/source/search filters.
Confirm that the matching RuneSchema and Helpy DLLs are installed together.
An expired page cursor should restart its query rather than append incompatible
pages. A genuinely unready registry may need startup to finish. Keep the
failure message and catalogue coverage; a blank icon is not the same as a
missing item definition. Do not remove item mods or save data as a cache fix.

### RS-SPAWN-001 — Spawn or placement waiting for collision {#rs-spawn-001}

The destination may not have streamed usable floor collision, or a native
placement check did not confirm success. Move close enough for the cell to
load, verify the displayed ground Z, and retain the actor/item path and
placement result. Queued placement is not confirmed placement. Do not force
an arbitrary Z or repeatedly create duplicates; stacked floors, bridges and
roofs require the appropriate nearby collision surface.

### RS-LOG-000 — Unfamiliar warning or error {#rs-log-000}

This entry has no more specific reviewed guide yet. Read its complete text
and the first preceding failure; export the relevant filtered entries and
record the versions, enabled mods and action that triggered it. Do not treat
this general code as a root-cause diagnosis. No logs are uploaded by Helpy.

## RuneSchema does not appear in the log

Check the UE4SS `Mods/mods.txt` file. The line must be:

```text
RuneSchema : 1
```

Confirm the RuneSchema DLL is in the expected `RuneSchema/dlls` location and
that the game was fully closed before the DLL was replaced.
Compare both DLL identities with the package you installed: the published ZIP
is `0.8.0.1`; the newer source/tag is `0.8.0.1b`. Mixed versions or an unexpected
older label can indicate a second installation or an old DLL.

## A LogicMod starts on a dedicated server

RuneSchema clears its client LogicMod handoff lists during
dedicated-server startup. Confirm the log shows
`[LOGIC-PAK][SERVER] Client ModActor handoff cleared.` If it instead reports
`[PARTIAL]`, preserve that line and check folder write access. Also check for
a separately installed copy under the game's `Content/Paks/LogicMods` or an
independent BPModLoaderMod; RuneSchema cannot disable those copies.

## A RuneSchema mod still loads after being disabled

Check `RuneSchema/mods/runeschema.txt`, not the UE4SS list used for RuneSchema
itself.

```text
MyMod : 0
```

Remove duplicate lines. RuneSchema treats any `0` for the same folder as
disabled, even when the letter case differs. Restart the game after changing
the file.

Also remove duplicate copies of the PAK from other mod folders or the game's
ordinary PAK location. Disabling one folder cannot disable a second copy stored
somewhere else.

## The PAK mounted but the item is missing

Check these in order:

1. Open the PAK and confirm the item's internal path.
2. Compare that path character for character with the `assets` or recipe file.
3. Confirm the object name appears after the final dot.
4. Confirm the cooked object is an item data asset, not only a Blueprint or mesh.
5. Add a direct empty `assets` reference to the item path.
6. Read the item registration summary for a rejection or duplicate identity.

Example direct reference:

```json
{
  "/MyMod/Items/ITEM_MyWeapon.ITEM_MyWeapon": {}
}
```

## Some items from a large PAK are missing

Confirm the missing items were actually cooked into the package. A recipe or
texture referring to an item name does not prove the item asset was included.

For a large catalogue, do not solve the problem by generating thousands of
empty `/assets` files. Confirm that the PAK contains the native item, recipe,
station, and unlock relationships that cause the game to load the content.
Compare the log's mounted and live-registration totals, and look for duplicate
persistence IDs or internal names.

Use a [cooked declaration or loose `registry` sidecar](COOKED-PAK-REGISTRY-MANIFEST.md)
for supported enrollment lanes. A loose sidecar can register an older PAK's
existing items without recooking it; it cannot supply missing assets or implement
a declaration-only family. Skills and SkillPerks are not active enrollment lanes.

By default, RuneSchema warns above 50,000 authored recipes. The warning is
advisory: RuneSchema continues loading them and does not disable the mod.
Advanced settings lets you turn the notice off or change its threshold.
In `settings.ini` under `[diagnostics]` (or legacy JSONC), use `largeRecipeWorkloadWarning` and
`largeRecipeWarningThreshold` for the same controls.

## The recipe reports a missing item

Use the full item object path in `ItemsConsumed` and `ItemsCreated`. The output
must already load as a valid item. A display name, filename, or short internal
name is not a substitute for the object path.

If the recipe uses a RuneSchema-created copy, confirm its resolved runtime path
and keep the provider mod above dependent mods in `runeschema.txt`. Numbered
filenames order files within a loader; they do not reorder loader phases.

## The recipe exists but is not in the station

Confirm:

- the table is a real crafting or processing table;
- the row name matches the intended station;
- a crafting station uses `Category`;
- a processing station uses the correct array, commonly `Recipes`;
- a custom table uses its full `DataTable` object path;
- the station itself loaded before testing the recipe.

## The station appears but cannot be used

The model and building piece may be working while the interaction is incomplete.
Check the station actor's parent, interaction component, station row, and user
interface reference against a working native station of the same type.

## A custom weapon equips but cannot attack

Check that the item, held actor, weapon family, animation pose, and attack family
all belong together. Test with the closest native attack family before adding
custom attacks. A visible mesh does not prove the combat setup is complete.

## The character says the save is corrupted

Do not keep retrying after changing the mod list repeatedly.

1. Close the game.
2. Back up the character.
3. Restore the last mod set that loaded successfully.
4. Start the game and wait for the content-registration summary.
5. Confirm every expected PAK and registry is ready.
6. Load the character once.
7. If it still fails, keep the complete `UE4SS.log` from that launch.

RuneSchema removes only identities that remain unresolved after enabled content
has loaded. If a required mod failed to mount, fix that failure before treating
its saved entries as truly missing.

## The game slows down during startup

Turn off verbose logging and broad diagnostics. Remove duplicate PAK copies.
Check for a mod that contains a very large number of unrelated assets under a
mounted mod root. Test half the enabled mods at a time while keeping the same
character backup.

Do not judge startup performance by repeatedly entering and leaving a world in
one launch. RuneSchema's full setup should occur once at game startup.

## The game crashes on world re-entry

Keep the log and crash dump from the same launch. Record whether first entry
worked, whether the crash happened while leaving or entering, and which mods
were changed. Restore the last stable build and mod list before testing again.

## Report checklist

Include:

- RuneSchema version;
- Dragonwilds store version;
- first entry or re-entry;
- full `UE4SS.log`;
- crash dump when available;
- `runeschema.txt`;
- recently changed mods;
- the exact item, recipe, station, or object path involved.

Review logs for account names, IDs, local paths and connection details before
sharing. Helpy's [bug-report form](HELPY-BUG-REPORTS.md) opens a prefilled GitHub
issue for review; it does not silently upload logs or submit an issue.
