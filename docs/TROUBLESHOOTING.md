# Troubleshoot RuneSchema and PAK mods

Work from the first failed step instead of changing several files at once. A
PAK can mount while an item inside it still fails to load, and an item can load
while its recipe still points to the wrong station.

## RuneSchema does not appear in the log

Check the UE4SS `Mods/mods.txt` file. The line must be:

```text
RuneSchema : 1
```

Confirm the RuneSchema DLL is in the expected `RuneSchema/dlls` location and
that the game was fully closed before the DLL was replaced.

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

If RuneSchema reports more than 1,000 authored recipes, the warning is
advisory. RuneSchema continues loading them; it does not disable the mod.

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
