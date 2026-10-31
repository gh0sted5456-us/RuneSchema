---
channel: experimental
promote_to: COMPATIBILITY.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Platform and multiplayer support

RuneSchema uses one mod format for Steam, Epic, and Game Pass. You can use the same JSON definitions across stores. The game program is different between
store versions, so the correct RuneSchema and UE4SS package still matters.

## Character saves

The useful character information is the same kind of game data on each store,
but the stores keep it in different places. Steam and Epic use normal local save
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

Building IDs have their own exchange during joining. RuneSchema includes them
in the first server announcement and retries the client acknowledgment up to
16 times, two seconds apart, while the owned player connection becomes ready.
Retries stop after a matching reply and do not repeatedly reload quest or
world snapshots. A temporary `BUILDINGS-PENDING` message means this exchange
is still finishing; it does not establish that your mod lists differ. The
`BUILDINGS-READY` message confirms the exchange completed. Each placement
still checks the selected ID against both building lists; an actual mismatch
remains blocked with `RS-NET-003`.

## Game updates

A game update can move content, rename fields, or change how a feature starts.
After an update:

1. keep important characters backed up;
2. confirm that RuneSchema supports the new game build;
3. test with a small mod set first;
4. read the log for unavailable features or missing content paths;
5. test both first entry and re-entry before continuing normal play.
