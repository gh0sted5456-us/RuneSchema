# World saves and mod removal on a server

Placed buildings belong to a **world** save; inventory items and many quest
records belong to a **character** save. Protect both before removing a
gameplay mod. A world-safe cleanup must leave unrelated vanilla structures
alone even if a removed station once stood on them.

## Before disabling a building or station mod

1. Stop the server completely.
2. Copy the server's `RSDragonwilds/Saved/SaveGames` folder to a separate
   backup location. Preserve its file names and backup files together.
3. Keep a copy of the old mod folder and its PAK triplets for rollback.
4. Change only that mod's line to `0` in `runeschema.txt`.
5. Restart and read the startup log before letting players enter the world.

Where possible, dismantle modded structures in a backed-up world before removing
their content. World and character cleanup are separate, so check the current
release’s world-cleanup behavior rather than relying on character-ID cleanup to
remove placed buildings.

## After startup

Check the server log for:

- whether the remaining mods and their PAKs mounted;
- whether the expected buildings still appear in the live building registry;
- whether any save-cleanup warning concerns a character or a world;
- whether the affected world still loads with unrelated structures intact.

Test a copy of the affected world with one client before returning to normal
play. Verify placed vanilla pieces, unrelated mod pieces, and world re-entry.
If the log reports an incomplete registry, fix registration or restore the
old mod set before trusting any cleanup decision. If a placed mod building
cannot be removed safely, keep the matching mod installed until you have a
verified repair path for that save.

Character cleanup is a separate path. Read
[save cleanup and safe removal](../SAFE-SAVE-AND-LEDGER.md) for its timing and
limits. A server and client must not race to write the same save file; always
stop the server before an external backup or restore.
