# RuneSchema 0.7.7.7m main release

This October 9, 2026 main build includes the tested Blueprint and Lua
companion paths. It also continues the work on dependable mod loading, safer
character saves, clearer status messages, and support for mods that bring
their own PAK content.

The Advanced Authoring & Tools page now uses one top-to-bottom workflow.
Choose a stage, then a content type when authoring; the selected tool uses the
full page width. Previous/Next controls move between stages, recent results
stay at the bottom, and structural schema export is available under All Loaders.
Within a loader, choose the starter format before searching and selecting a
record, then review fields, build a preview, check dependencies, and export.
No authored files are activated merely by opening or browsing this page.

Make a backup before testing a new game build, a new RuneSchema build, or a
large change to your mod list.

## What players will notice

- A mod marked `0` in `RuneSchema/mods/runeschema.txt` stays disabled. Letter
  case and duplicate lines cannot turn it back on.
- Mods load in the order shown in `runeschema.txt`. RuneSchema no longer
  rearranges newly found mods alphabetically.
- RuneSchema prepares its content once when the game starts. Returning to the
  menu and entering another world does not repeat the full setup.
- Loaded PAK items, recipes, quests, journal entries, magic, and equipment
  effects are checked against the game lists that actually use them.
- In multiplayer, the server provides the registry baseline for the session.
  Clients report clear per-mod differences without being blocked when they
  have extra content or are missing optional content.
- Client presentation-route notices identify the affected mod and are reported
  once per loaded manifest instead of repeatedly warning during normal play.
- The log now gives short totals for content found in PAK files and content
  created through RuneSchema files.
- Character menu additions can share the available grid more reliably instead
  of pushing entries into broken rows or columns.
- Mod authors can change supported menu text, fonts, and colors. The included
  golden interaction-text example is a starting point.
- Processing-station recipe placement is now read back from the live station
  row. Missing replacement targets and rejected native array writes are
  reported as errors instead of clean recipe loads.

## Save cleanup is narrower and safer

RuneSchema does not delete a saved entry just because it came from a mod. It
first lets enabled mods and PAK files load. It then asks the live game lists
whether each saved item, recipe, quest, or journal entry still exists.

An entry is removed only when it cannot be found in the matching live game
list. This is useful after uninstalling a mod because the missing item no longer
blocks the character from loading.

Cleanup runs at the first character-load point for that game launch. It does
not repeatedly clean the same character every time the player moves between a
world and the main menu. If nothing is missing, RuneSchema leaves the character
data unchanged.

[Read the full save-safety guide](SAFE-SAVE-AND-LEDGER.md).

## Clearer PAK and RuneSchema roles

A mounted PAK makes cooked files available. For ordinary mods, RuneSchema can
reference those cooked items and recipes by their full object paths, place
recipes in stations, and edit a cooked mod DataTable through `/raw`.

Very large content libraries should keep their items, recipes, station
relationships, and unlock relationships inside the cooked PAK. They should not
need thousands of empty RuneSchema files merely to make every path load.
RuneSchema can still perform the small integration work the mod asks for, such
as adding consumable packs to enemy loot tables.

When RuneSchema receives more than 1,000 authored recipe definitions, it prints
one warning with the total. Loading continues normally without a cap or refusal.
The warning simply tells the author that a native cooked-PAK layout may be
easier to maintain.

RuneSchema reports mount, resolution, and live-registration results separately
so an author can distinguish a missing PAK from a bad object path.

[Learn how to create PAK content](PAK-CONTENT-GUIDE.md).

## Blueprint and Lua companions stay inside a RuneSchema mod

Mod authors can keep optional Blueprint startup packages under
`logicmods/<PackageName>/` and UE4SS Lua code under `ue4ss/scripts/main.lua`
inside their RuneSchema mod. RuneSchema mounts the cooked package and uses its
own Lua helper to start its `ModActor`; it starts a Lua companion
once at game launch when the same mod is enabled.

An ordinary `paks` folder still mounts cooked content but no longer asks
BPModLoaderMod to start every package. This cuts down unwanted startup checks
and log noise. The log shows one LogicMod submission summary, while a generated
list keeps the exact package names for troubleshooting. This path leaves
BPModLoaderMod untouched. A mod marked `0` in
`runeschema.txt` does not contribute PAKs, LogicMods, or Lua code on the next
launch.

If your existing mod needs a `ModActor`, move its complete package folder from
`paks` to `logicmods`; do not leave duplicate copies. Restart after changing
the mod list.

[See both optional folder layouts](PSEUDO-LOADERS.md).

## Bridges are included in the release package

The 0.7.7.7m release includes these bridges in one RuneSchema ZIP: the complete
shared registry bridge PAK and the RSNetworking bridge. They are separate
plugins, so players can turn either one off without disabling RuneSchema.

The registry bridge gives PAK authors a shared base for declaring the items,
recipes, spells, and attacks their own cooked mod supplies. RSNetworking gives
mods a shared path for supported multiplayer presentation and identity work.
Neither bridge invents missing content, replaces the game's save format, or
forces RuneSchema's optional weapon fallback on.

Keep a bridge enabled when an installed mod requires it.

[See what each optional bridge does](OPTIONAL-BRIDGES.md).

## What RuneSchema does not do

RuneSchema cannot repair a PAK that was cooked with missing files or incorrect
internal paths. It also cannot make two mods safely replace the same cooked file.
When mods overlap, the author must choose unique paths or provide a deliberate
compatibility patch.

RuneSchema does not restore items that were previously removed from a save.
Reinstalling a removed mod makes its content available again, but it does not
recreate possessions or progress that no longer exist in the save.

## Recommended update steps

1. Close Dragonwilds completely.
2. Back up at least one test character.
3. Replace RuneSchema's program files with the new package; preserve your
   existing `settings` and `mods` folders. In `settings.jsonc`, keep
   `bpModLoader.luaActorLoader` enabled and `patchScript` disabled.
4. Review `RuneSchema/mods/runeschema.txt` before starting the game.
5. Start with the same mods that were used for the last successful save.
6. Read the first RuneSchema summary in `UE4SS.log`.
7. Enter a test world, return to the menu, and enter it again.
8. Only then test removing or disabling a mod.

## Information to include with a report

Please include:

- Steam, GOG, or Game Pass;
- whether the problem happened on first entry or re-entry;
- the full `UE4SS.log` from that launch;
- the relevant lines from `runeschema.txt`;
- the names of recently added, updated, disabled, or removed mods;
- whether the same character loads after restoring the previous mod list.
