# Current release

RuneSchema uses one `main.dll` for Steam/GOG and Game Pass. It detects the
running storefront and selects the matching game support automatically.

## Stability and compatibility

- Steam/GOG and Game Pass use separate game-function data where their
  executables differ.
- Both storefronts follow the same loader, ownership, and save-cleanup rules.
- An unavailable optional game function disables only the feature that needs
  it. Other loaders continue where it is safe to do so.
- Plugins remain optional. A plugin version mismatch does not prevent
  RuneSchema from loading.
- Loader folders and nested organizational folders are case-insensitive.

## Save cleanup

RuneSchema does not maintain a player-content ledger or restore history.
Enabled mods and cooked content register first. Before character hydration,
RuneSchema removes applicable saved identities that do not resolve in the
completed live registries.

The current cleanup covers RuneSchema-owned:

- carried and equipped items;
- recipe unlocks;
- quest progress;
- journal entries; and
- lore entries.

Steam/GOG and Game Pass use the same cleanup decision through their
storefront-specific save lanes, then let the game save normally. Xbox Game Save
containers are never edited directly. Removing and reinstalling a mod is a fresh
installation; previously removed state is not restored.

## Loader improvements

- `/assets` supports compatible item clones, cooked-content declarations,
  unlock items, and direct `DA_` asset edits.
- `/raw` can create or patch rows in a DataTable selected by short name or exact
  cooked path.
- `/recipes` can target crafting stations, timed processing stations, vanilla
  merchants, RuneSchema vendors, and compatible custom station tables.
- Runtime-created item clones are registered before a processing recipe is
  accepted, preventing an incomplete item from entering a timed queue.
- Recipe placement and recipe unlocking remain separate choices.
- `/journal` and `/lore` support authored entries, grouping, placement, normal
  unlocking, and save cleanup on both storefronts.
- `/buildings` supports new build-menu entries, inherited or replaced costs,
  cooked replacement actors, processing-station fuel settings, and Base Builder
  imports.
- `/equipment` supports equipped effects, skills, utility actions, Surge-style
  movement, and Shadowveil-style presentation when the required game function
  is available for the detected storefront.
- `/registry` joins server-authoritative actions with the matching client
  presentation data and reports duplicate keys.
- Vendor categories keep their authored order and can use power-level,
  time-of-day, quest, and timeout conditions.
- Player, NPC, resource, and building scale is applied from its authored base
  value instead of multiplying again after a reload or respawn.

## Character creation

- Hair, facial hair, body, face, skin, eyes, and other supported character-menu
  options can be added or updated through `/raw` and direct `/assets` `DA_`
  edits.
- New authoring files need only the target `DA_` path and the fields being
  changed. A web schema URL or repeated mod identifier is not required.
- Character-menu changes are reapplied when the game rebuilds the menu data.
- Automatic RuneSchema appearance saving remains off by default. A choice saved
  through the game's character editor remains a normal game action.

## Multiplayer and time conditions

- The server remains responsible for inventory, purchases, quests, spawning,
  buildings, AI, drops, and events.
- Clients render the cooked assets they have installed and receive replicated
  world state.
- Time-of-day conditions are shared by vendors, NPCs, spawns, events, quests,
  dialogue, rewards, drops, and visual effects where each loader supports them.

## Helpy and diagnostics

- Helpy is optional and opens from its saved catalogue instead of scanning the
  entire game every time.
- Search, filtering, pagination, and icon work are loaded as needed.
- Normal logging shows loader totals, warnings, and failures without repeating
  successful low-level operations.
- Advanced logging adds bounded examples for troubleshooting.

## Default persistence settings

```jsonc
"persistence": {
  "characterCustomization": false,
  "journal": false,
  "recipes": false,
  "quests": true
}
```

Turning off one of these settings does not disable its loader. It prevents
RuneSchema from making new progress in that category permanent.

## Packages

- **Universal** includes RuneSchema core and the optional bundled plugins.
- **Core** includes RuneSchema without optional plugins.
- The same RuneSchema DLL supports both storefronts; use the UE4SS runtime made
  for the installed game build.

See the [Authoring Guide](AUTHORING-GUIDE.md),
[Loader Reference](LOADER-WALKTHROUGHS.md),
[Examples](EXAMPLES.md), and
[Save Cleanup](SAFE-SAVE-AND-LEDGER.md) for current usage.
