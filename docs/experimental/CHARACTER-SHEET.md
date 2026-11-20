---
channel: experimental
promote_to: CHARACTER-SHEET.md
---

!!! info "Experimental · in development"
    Built on `experimental` and contract-tested. Game Pass import, live J4SSaves
    store changes and Steam Cloud uploads still need in-game acceptance.
    [Experimental overview](index.md).

# Helpy character sheet

**Players → Save editor** opens a saved copy of one character. Choose a save, or
leave **Follow current character** on. Edits stage on an isolated copy and are
exported after review. The live character and the game's save slots are not
changed.

| Page | What it does |
| --- | --- |
| **Inventory** | The saved inventory beside the Item Bay. Drag an item across to stage an addition. |
| **Appearance** | Name, appearance options and the game's colour palettes. |
| **Loadout** | The saved equipment, read only. Change gear on the live character from its gear menu. |
| **Skills** | Saved skill levels, using the game's experience thresholds. |
| **Deliveries** | This character's Helpy delivery switch, and any mod that pauses deliveries to it or its world. |
| **Save sections** | Mod data that J4SSaves keeps with the character: back up, restore or clean. |
| **Transfer & export** | Send the character to Steam/Epic or Game Pass, or export an edited copy. |

The character and item previews are retired.

## Deliveries

The switch here is the same one as in **Settings → Deliveries**. With it off,
Helpy refuses deliveries you make to this character. A server host can still
give you items. The page also lists each mod pause that applies to this
character or its world, and whether you still honour that mod. Manage mods in
**Settings → Deliveries**. See [Helpy delivery flags](HELPY-DELIVERY-FLAGS.md).

## Save sections (J4SSaves)

[J4SSaves](https://github.com/gh0sted5456-us/J4SSaves) lets mods keep their own
data with a character. Each section belongs to one mod, such as
`jonesing4space.jbm`. It travels inside the character save, and J4SSaves also
keeps a local copy in `%LOCALAPPDATA%\J4SSaves\J4SSaves.sqlite3`.

The page lists each section's size, schema and revision, and where it is kept:
the save, J4SSaves, or both. Tick sections, then:

- **Back up selected** writes them to the character's profile files, in
  `%LOCALAPPDATA%\RSDragonwilds\RuneSchema\runeschema.profile\characters\<GUID>\sections`.
  A section kept only by J4SSaves is backed up from J4SSaves.
- **Restore this backup** puts a backup's sections back into the save copy. Each
  restored section gets a revision newer than any copy Helpy can see, so it wins
  on the next load.
- **Clean selected** removes sections from the save copy. The mods lose that
  data unless you restore a backup.

Restores and cleans are ordinary staged edits: review and export the copy to
keep them. When J4SSaves 1.1 or later is loaded, **Also apply to the J4SSaves
store** makes the same change in J4SSaves. Without it, J4SSaves keeps its own
copy and may embed it again on the next save. A cleaned section leaves a
tombstone in J4SSaves, so an older copy in another save is not imported again.

## Steam/Epic and Game Pass

Both versions store the same character data. Game Pass keeps it in its own save
container. Transfer the saved copy; export or discard staged edits first. Use the
same gameplay mods on both installs.

- **Send to Steam / Epic** writes a save file to
  `%LOCALAPPDATA%\RSDragonwilds\Saved\SaveCharacters`. If Steam already has this
  character, Helpy asks before replacing it and backs up the old file to the
  character's profile `backups` folder first. Close the Steam game, or return to
  the main menu, before replacing a character it has loaded. Steam Cloud uploads
  the file on its next sync.
- **Send to Game Pass (J4SSaves)** queues the copy at
  `%LOCALAPPDATA%\J4SSaves\import-character.json`. Start the Game Pass game with
  J4SSaves and open character selection; J4SSaves creates a new slot and confirms
  the import in `WinGdkPreview.log`. A Game Pass slot that already has this
  character blocks the import. Delete `import-character.json` with the game
  closed to cancel.

Helpy never writes Game Pass save containers itself.

## Search by source

Type `source:` and a mod or source name in an item search, for example
`source:JBM helmet` or `source:"Colors of Money" coin`. It selects the same
source as the **Source** list, and the rest of the text is searched as usual.
`source:vanilla`, `source:plugins`, `source:mods` and `source:all` pick the
groups. A partial name works when only one source matches. Choosing in the
**Source** list updates a typed prefix to match.
