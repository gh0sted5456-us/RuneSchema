---
search:
  exclude: true
---

# Backup-first pruning candidate — 2026-10-22

Historical implementation note. For current behavior and removal instructions,
use [save safety](SAFE-SAVE-AND-LEDGER.md) and [settings](SETTINGS-INI.md).

Companion Recovery is retired from runtime load, registration and world-cleanup
paths. The Quick Settings toggle is replaced by a backup-first explanation.
Old configuration keys and archives remain compatible but cannot restore records
or disable mandatory pre-prune backups.

Before character JSON replacement, RuneSchema writes and reads back the exact
original payload. Post-hydration quest cleanup instead copies matching native
provider files by character GUID and confirms they did not change during copying.
These are the provider's last written saves, not an export of unsaved live state.
Existing startup/world backups also verify before mutation. Failure is a genuine
cleanup blocker, with no automatic restore or destructive backup rotation.

The Ashhide regression covers the exact removed provider identity in active,
completed and unfinished states, tracking and owned location markers, while
preserving enabled, pending and unrelated records. The native fallback validates
reflected layouts and authority, backs up before notification, sends the retained
quest list while the old list is still available for the native removal diff,
then updates authority tracking. Its disk persistence waits for normal game save.
Failure retries on a subsequent hydration rather than rescanning provider files
on every poll.

The combined candidate also includes the separate Quest Board activation-health
fix, centered/clamped details, individual source-provider filters, and the prior
icon transport/mapping-reuse fixes. The Dye_Quests furnace save-exclusion failure
is not fixed. Steam native notification behavior was inspected statically;
Game Pass independently denied executable read access. No live character/world
pruning or restoration has been performed by the agent.
