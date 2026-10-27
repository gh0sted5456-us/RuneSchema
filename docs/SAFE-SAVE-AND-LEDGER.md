# Save cleanup and safe mod removal

RuneSchema helps a character keep loading after you remove a mod by checking
saved content against the game’s current registered lists. Folder changes alone
do not mark an item as missing, and this check works without a separate history
file.

## What happens during character loading

1. Enabled RuneSchema mods and PAK files load.
2. RuneSchema adds valid items, recipes, quests, and journal entries to the
   matching live game lists.
3. RuneSchema checks the character information just before the game uses it.
4. A saved identity is removed only when it is absent from the matching complete live list.
   RuneSchema verifies a whole-save backup before committing a cleanup.
5. Each section is independent. An incomplete recipe registry or unsupported
   quest section does not prevent verified cleanup of other sections. Unknown
   identities are retained wherever the applicable list is not certified.
6. If nothing is missing, RuneSchema makes no change.

An unfamiliar Inventory, PersonalInventory or Loadout container is preserved
without blocking its readable siblings. Valid Inventory and Loadout sections
still share slot-removal dependencies. Nonstandard equipment indices are never
truncated to an unrelated slot. Opaque quest records retain their tracking and
location dependencies while other verified orphan quests can be removed.
Unfamiliar rows/sections appear in skipped-section diagnostics, with repeated
row warnings grouped per section. Unknown fields remain untouched; this does
not claim to decode future save formats or repair unsupported world/SPUD data.

The JSON check occurs at the first character-load point for that game launch.
A separate post-hydration check handles RuneSchema-owned orphan quests whose
native state becomes available later. Active or unfinished status does not
protect an incomplete quest whose identity is absent from a complete registry.
Completed native quest history and RuneSchema completion-phase records are
retained automatically; explicit manual progress resets remain separate. Pending
registrations and unrelated quests remain protected. Native client notification
receives the retained list before authority rows are replaced, so removed quests
can disappear from the UI; the game persists this at its next normal save.

Normal/Quick Launch clients and dedicated servers first request relaxed native
loading, then use the verified acceptance fallback only if native code rejects
the load. A character-specific game notification reports cleanup, skipped
uncertain sections, or an attributed fallback once the player is ready. Details
remain in RuneSchema.log. Unattributed native calls produce only a generic
session notice on the local client, never a remote player's character receipt.
For a truncated or damaged save, use a known working backup. Fallback acceptance
can let loading continue without confirming every record, so check the character
and world before continuing normal play.

## What the check protects

The check is designed to let a character load after a mod was removed and its
saved items or progress no longer exist. It covers supported saved identities
such as items, recipes, quests, and journal or lore entries.

RuneSchema does not remove a valid entry just because it came from a PAK. If the
PAK item loaded and joined the correct live list, its identity remains valid.

## Steam, Epic, and Game Pass

The stores keep saves differently, but RuneSchema uses the same point where the
game prepares character information. RuneSchema does not directly edit an Xbox
save container. It makes the narrow change while the game is loading the
character, then Dragonwilds and the active store handle normal saving.

## Limits

- RuneSchema does not automatically restore removed content; a verified backup
  can be restored manually, rolling back progress since that backup.
- Reinstalling a removed mod does not recreate lost possessions or progress.
- A PAK that failed to mount can make otherwise valid content appear missing.
  Fix mount and registration errors before testing cleanup.
- Placed buildings belong to the world save. Removing a building mod needs more
  care than removing an inventory item.
- Post-hydration quest pruning requires an exact-GUID local character backup.
  If a dedicated server has no matching character provider file, this fallback
  defers instead of mutating live state without a backup. Xbox provider files
  are read/copied only; native saving remains owned by the game.

## Recommended removal process

Cleanup removes supported missing identities; it is not a generic migration
for changed capacities or item rules. Before reducing chest/inventory slots,
empty the affected slots. Before lowering stack limits, split oversized stacks.
Keep the old mod version and a whole-save backup. Re-enabling a mod after a
pruned save does not replay its removed progress. See the
[author testing checklist](modders/testing-and-compatibility.md).

1. Back up the character and world.
2. Close the game.
3. Set the mod to `0` in `RuneSchema/mods/runeschema.txt` or remove its folder.
4. Restart the game.
5. Wait for RuneSchema to finish loading all remaining content.
6. Load a backed-up test character.
7. Read the cleanup warning and confirm the removed identities belong to the
   content that is truly gone.
8. Save and test world re-entry before continuing normal play.
