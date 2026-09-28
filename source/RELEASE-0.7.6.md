# RuneSchema 0.7.6

- Corrects lifecycle accounting so a section scheduled for a later engine
  phase, such as `/spawns`, does not falsely mark its mod `PARTIAL`.
- Journal/lore and recipe definitions and unlocks are session-only. Quest
  persistence remains configurable.
- Separates schema unlock policy from persistence. `Unlock` controls automatic
  runtime delivery. Journal/lore entries can remain session-only on a verified
  native save lane, while recipe unlocks use Dominion's non-persistent set.
- Marks RuneSchema recipes learned through native recipe-unlocker consumables
  as session-only too.
- Adds a documented `/assets` example that clones a native recipe-unlocker
  consumable and points `RecipesToUnlock` at the Dawnveil Paladin Plate recipe.
- `RecipesToUnlock` now accepts an authored recipe name, an exact
  `ModID:RecipeName`, or a cooked `RecipeData` path. Duplicate unqualified names
  are rejected instead of crossing mod ownership.
- Adds verified `/buildings.Overrides` routing for independent station names,
  terrain-capable placement profiles, shelter interaction policy, station-row
  relationships, and processing-rate multipliers.
- When quest persistence is disabled, definitions remain registered for
  references and presentation, but RuneSchema quest actions are blocked because
  Dominion quest progress is inherently save-backed.
