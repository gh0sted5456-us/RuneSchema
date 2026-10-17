# Troubleshoot RuneSchema and PAK mods

Work from the first failed step instead of changing several files at once. A
PAK can mount while an item inside it still fails to load, and an item can load
while its recipe still points to the wrong station.

## Helpy Live Log and help codes

Open **Live Log** in Helpy's left navigation to read this installation's
`RuneSchema/RuneSchema.log`. Search text or an `RS-` code, filter by severity
or subsystem, pause reading, follow the newest entries, and select a row for
its full message. **Clear view** clears only the viewer, never the log file.
The recent history is bounded to 4,000 entries / 2 MiB of text; first opening
reads the recent tail, not an unlimited archive. Log rotation starts a fresh view.

**Export filtered log** saves the currently retained matching entries, with
their help codes, to a local UTF-8 file. It is not an export of an entire
previous session. Open the log folder for the original and `.previous` file.
Review player names, IDs and local paths before sharing an export.

Clickable help codes open the matching section below in your browser. No log
text, player identity or file path is sent in the link. Codes are a Helpy
interpretation of known diagnostic signatures; the original log is unchanged.
An unfamiliar warning uses the general guide, not a guessed diagnosis.

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
Automatic destructive cleanup remains disabled when certification is missing.
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
For the current main package, confirm RuneSchema and RSMK identify `0.8.0.0`.
An older label means an older DLL is still being loaded.

## A LogicMod starts on a dedicated server

The 0.7.7.7 build clears RuneSchema's client LogicMod handoff lists during
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

By default, RuneSchema warns above 50,000 authored recipes. The warning is
advisory: RuneSchema continues loading them and does not disable the mod.
Advanced settings lets you turn the notice off or change its threshold.
In `settings.ini` under `[diagnostics]` (or legacy JSONC), use `largeRecipeWorkloadWarning` and
`diagnostics.largeRecipeWarningThreshold` for the same controls.

## The recipe reports a missing item

Use the full item object path in `ItemsConsumed` and `ItemsCreated`. The output
must already load as a valid item. A display name, filename, or short internal
name is not a substitute for the object path.

If the recipe uses a RuneSchema-created item copy, put the item file before the
recipe file and keep the item mod above dependent mods in `runeschema.txt`.

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
