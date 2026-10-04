# Platform and multiplayer support

RuneSchema uses one mod format for Steam, GOG, and Game Pass. Authors do not
need separate JSON files for each store. The game program is different between
store versions, so the correct RuneSchema and UE4SS package still matters.

## Character saves

The useful character information is the same kind of game data on each store,
but the stores keep it in different places. Steam and GOG use normal local save
files. Game Pass uses the Xbox save container.

RuneSchema does not move files between stores and does not directly rewrite an
Xbox container. It checks the character information while the game is loading
it, then lets the game and the store save normally.

## Multiplayer

Every player should use the same gameplay-changing mod set when a mod adds:

- items or equipment;
- recipes or stations;
- quests, dialogue, or events;
- enemies, drops, or vendors;
- magic or equipment behavior.

Visual-only changes may not be needed by a dedicated server, but every client
that must display a new mesh, icon, material, sound, or interface element needs
the matching PAK content.

The host or server decides gameplay results. Clients still need matching item
and recipe information so those results can be shown and used correctly.

## Game updates

A game update can move content, rename fields, or change how a feature starts.
After an update:

1. keep important characters backed up;
2. confirm that RuneSchema supports the new game build;
3. test with a small mod set first;
4. read the log for unavailable features or missing content paths;
5. test both first entry and re-entry before continuing normal play.

