# Save cleanup and safe mod removal

RuneSchema checks saved mod content against the game lists that are live for the
current launch. It does not guess that an item is missing because its mod folder
changed, and it does not depend on a separate history file.

## What happens during character loading

1. Enabled RuneSchema mods and PAK files load.
2. RuneSchema adds valid items, recipes, quests, and journal entries to the
   matching live game lists.
3. RuneSchema checks the character information just before the game uses it.
4. A saved identity is removed only when it is absent from the matching live list.
5. If nothing is missing, RuneSchema makes no change.

This check occurs at the first character-load point for that game launch. It is
not repeated every time the player moves between a world and the main menu.

## What the check protects

The check is designed to let a character load after a mod was removed and its
saved items or progress no longer exist. It covers supported saved identities
such as items, recipes, quests, and journal or lore entries.

RuneSchema does not remove a valid entry just because it came from a PAK. If the
PAK item loaded and joined the correct live list, its identity remains valid.

## Steam, GOG, and Game Pass

The stores keep saves differently, but RuneSchema uses the same point where the
game prepares character information. RuneSchema does not directly edit an Xbox
save container. It makes the narrow change while the game is loading the
character, then Dragonwilds and the active store handle normal saving.

## Limits

- RuneSchema cannot restore content that was already removed from a save.
- Reinstalling a removed mod does not recreate lost possessions or progress.
- A PAK that failed to mount can make otherwise valid content appear missing.
  Fix mount and registration errors before testing cleanup.
- Placed buildings belong to the world save. Removing a building mod needs more
  care than removing an inventory item.

## Recommended removal process

1. Back up the character and world.
2. Close the game.
3. Set the mod to `0` in `RuneSchema/mods/runeschema.txt` or remove its folder.
4. Restart the game.
5. Wait for RuneSchema to finish loading all remaining content.
6. Load a backed-up test character.
7. Read the cleanup warning and confirm the removed identities belong to the
   content that is truly gone.
8. Save and test world re-entry before continuing normal play.
