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

The **draft bar** above the pages is the one place to finish: it says whether
the copy has changes that are not exported yet. **Review & export** lists every
staged change and saves the edited copy as a new file. **Discard changes**
reopens the same save without them.

| Page | What it does |
| --- | --- |
| **Inventory & loadout** | Helpy delivery status, the saved equipment (read only) and the saved inventory beside the Item Bay. |
| **Appearance** | Name, appearance options and the game's colour palettes. |
| **Skills** | Saved skill levels, using the game's experience thresholds. |
| **Save data** | Mod save sections (J4SSaves), and transfer to Steam/Epic or Game Pass. |

To add an item, select it in the Item Bay, or drag it onto the inventory. Set
the quantity and choose **Add to draft**. To rename the character, edit the
name and press Enter. Change gear on the live character from its gear menu.

The character and item previews are retired.

## Deliveries

The **Helpy deliveries** header on *Inventory & loadout* shows whether
deliveries to this character are on, off, or paused by a mod. Open it for the
switch, which is the same one as in **Settings → Deliveries**. With it off,
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

These are on the **Save data** page. Both versions store the same character
data. Game Pass keeps it in its own save container. Transfer the saved copy;
export or discard staged edits first, from the draft bar. Use the
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

## Search syntax

Item searches (the Item Bay, the save editor's bay and live inventory) and the
spawn tools' creature and resource searches share one syntax. Hover a search
box for a reminder.

| Query | Finds |
| --- | --- |
| `sword shield` | Items matching every word, as before. |
| `sword OR axe`, `sword \| axe` | Either word. |
| `-rusty`, `NOT rusty`, `!rusty` | Leaves out a word. |
| `(sword OR axe) -rusty` | Groups with parentheses. NOT applies first, then AND, then OR. |
| `"iron sword"` | The exact phrase. |

Operators are upper case, so a lower-case `or` or `not` is an ordinary word.
An unfinished query, such as a missing `)`, still searches what it can. In live
inventory, plain words now match anywhere in an item's name and details; use
quotes for the old exact match.
