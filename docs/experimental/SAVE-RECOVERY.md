---
channel: experimental
promote_to: SAVE-RECOVERY.md
---

# Save Recovery

> **Experimental:** this guide describes the local test candidate. It has not been promoted to the stable release.

Open **Save Recovery** in Helpy's navigation to browse whole-character snapshots. World snapshots and restoration are an additional experiment, disabled by default. Choose the runtime that created the backup, then select a snapshot to review its identity, backup reason and recorded mods. Hovering a row shows the same information beside the pointer. Narrow windows use **Backups** and **Details** tabs instead of compressing both panes.

## What is stored

Snapshots contain the complete save payload and a small metadata file. The metadata records the character name or world slot, snapshot time, last observed entry time when known, and the IDs, names and versions of RuneSchema mods active when the snapshot was recorded. An active mod record does not prove every listed mod contributed data to that save.

**Mod files are never included.** Players must reacquire and enable required mods themselves. Restoring a save does not download mods, change the mod order or bypass ordinary missing-content cleanup. Older backups can have unknown dates, names or mod records; Helpy does not invent them.

Backups live under `%LOCALAPPDATA%\RSDragonwilds\RuneSchema\core\backups\<runtime>\characters` or `worlds`. Identical save bytes reuse their SHA-256 archive rather than creating another copy. Different snapshots are retained; this first version has no automatic retention deletion.

## Automatic entry snapshots

Turn on **Automatic snapshots at character entry** to preserve the selected character before loading. It is off by default and saved separately for each runtime. It adds no periodic save scan during play. Character previews alone do not authorize a snapshot or cleanup.

Backups made **before cleanup** remain mandatory regardless of this toggle. The reasons shown on one snapshot can include both **Automatic** and **Before cleanup** when the bytes match.

## Optional world recovery

Open **Settings → Helpy → Saves → Experimental recovery** and enable **World save recovery (experimental)** for the assigned runtime. Return to Save Recovery and press **Refresh** to unlock world entries. With this setting enabled, the automatic-entry toggle covers both characters and worlds.

When disabled, RuneSchema does not create the new indexed world snapshots or apply queued world restores. Existing archives and pending requests are retained. Re-enable the experiment to review or cancel a pending request before loading that world. The setting is stored as `ExperimentalWorldRecovery` in that runtime's `characters/settings.json`; it does not enable blanket cleanup.

The existing verified rollback backup before actual world cleanup remains available independently. It is created only when cleanup will change a world. An optional recovery archive or metadata failure does not invalidate the world or prevent cleanup after that rollback backup has verified.

Normal cleanup is scoped to the selected character and local world at entry.
The **Blanket save cleanup (fallback)** setting explicitly enables broader
cleanup; keep it off for the normal selected-save behavior. On Game Pass, world
cleanup examines the native archive being loaded and checks expected modded
building IDs against that world's saved pieces. A verified enabled definition
is protected while its world/menu registration is pending. A readiness message
about a definition is not evidence that the world contained or lost that piece.

## Restore a snapshot

1. Close Dragonwilds and any dedicated server normally. Open Helpy independently.
2. Select the correct runtime and press **Refresh**.
3. Select the exact character or world snapshot. Review its GUID or world slot and reinstall any required mods.
4. Press **Restore save**, review the confirmation, and confirm the restore.
5. Launch the game and check the restored progress before discarding any backup.

Steam and Epic character recovery requires an existing, unique save with the same character GUID. World recovery requires the matching existing world slot. Dedicated-server Helpy restores server worlds; each player restores their own character from their own installation. This version does not guess new filenames for deleted characters or worlds.

The existing file is backed up before replacement. A changed current file, incorrect identity, damaged backup, redirected path or active game prevents the offline replacement. Whole files are restored; inventories and building lists are not reconstructed.

### Game Pass

Game Pass's WGS containers remain game-owned. **Queue restore for next entry** schedules a one-shot restore for the exact character GUID or world slot. RuneSchema verifies and backs up the incoming native payload before applying the selected snapshot at that entry boundary. It does not edit WGS container files directly.

The **Loaded** status means the native loader received the restored payload. It does not claim the game has persisted it yet. Let the game save normally and verify the result. An interrupted or failed claimed request becomes **Needs review** and is not replayed automatically. With the game closed, use **Cancel restore request** to cancel a pending or review-required request; backups are retained.

## Older backups and diagnostics

Existing content-addressed character archives are listed even when they predate metadata. New world snapshots are indexed by an exact slot and hash. Older adjacent world `.bak` files and ambiguous legacy Game Pass world archives are not automatically assigned to a world in this first version; retain those files for manual recovery.

- [RS-SAVE-003](../ERROR-CODES.md#rs-save-003): the selected entry snapshot was verified.
- [RS-SAVE-004](../ERROR-CODES.md#rs-save-004): a queued snapshot was loaded once; later saving remains game-owned.
- [RS-SAVE-005](../ERROR-CODES.md#rs-save-005): review the underlying backup or recovery failure.

The recovery page uses a bounded background scan when opened or refreshed. Large worlds still require file I/O when a snapshot or restore is requested; verification reads in small blocks instead of allocating another whole-world copy.
