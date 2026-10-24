# Example library

Use the smallest example that matches the problem. Examples are references, not
files installed by the public runtime.

## Override vanilla data

### Cape stat overrides

The Capes examples patch existing rows in `DT_WearableEquipment`. They are a
good reference for small, independent `/raw` overrides.

- [Attack cape override](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/Capes/raw/DT_ITEM_Cape_Attack_override.json)
- [Trimmed Magic skillcape override](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/Capes/raw/DT_ITEM_Cape_Trimmed_Skillcape_Magic_override.json)

Use this pattern when the row already exists and only a few fields change.

## Patch an existing Blueprint or widget

### Character-creation labels

[Fixed Menu](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/FixedMenu/blueprints/character_creation_text.jsonc)
changes labels on the existing character-creation widget.

It demonstrates a focused `/blueprints` patch without creating a new
Blueprint class.

### Live widget behavior

[Restore Appearance](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/RestoreAppearance/blueprints/10-RestoreAppearance.jsonc)
shows `$RuntimeWidget` changing live CommonUI widget state and binding an
existing button event to an existing native menu function.

## Character customization

### Current direct authoring

- [Add a hair menu option](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/CharacterCustomization/assets/domains/character_customization/hair-menu.json)
- [Unlock vanilla beards with `$MergeWhere`](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/CharacterCustomization/assets/domains/character_customization/unlock-vanilla-beards.json)
- [Character customization definition](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/CharacterCustomization/character_customization/hair.json)

### Registry-patch v1 form

[Unlock Vanilla Beards registry-patch sample](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/CharacterCustomization/registry-patch-v1/10-UnlockVanillaBeards.sample.jsonc)
shows the older transaction envelope used by MoreHair.

The sample is intentionally short. The original mod repeats the same merge
operation for the remaining beard rows.

Use the direct `DA_` form for new work unless the registry-patch transaction
model is specifically needed.

## Quests and Quest Boards

Follow [Build a quest and put it on a Quest Board](modders/quests-and-questboards.md)
for numbered instructions, a quest definition, a bound acceptance item and a
crafting-board recipe. The guide explains optional dialogue turn-in, cancellation,
custom board presentation and authority checks.

These are mock-up authoring starters. Replace sample identities and verify the
installed provider; they are not shipped gameplay content.

## Building clones

- [Cooked prop and building example](https://github.com/gh0sted5456-us/RuneSchema/blob/main/docs/examples/BuildingClone/buildings/10-CookedProps.jsonc)

The building example covers a new piece or prop. The
[buildings walkthrough](reference/loaders/buildings.md) explains cost replacement,
processing overrides and the station/recipe tie-ins.

## Recipe unlock items

Use the [assets walkthrough](reference/loaders/assets.md) for compatible item
clones and the [recipe discovery guide](RECIPE-DISCOVERY.md) for native unlock
conditions. Reference the exact recipe identity; a quest-acceptance scroll is not
a recipe unlock item.

## Crafting and processing placement

- Use `Category` for a named crafting or merchant category.
- Use `Array: "Recipes"` for a timed processing station.
- Use an exact `DataTable` path for a custom cooked station table.
- Reference an `/assets` clone by its RuneSchema object path. RuneSchema checks
  that the clone is fully registered before accepting a timed output.

See [Create stations and recipes](STATIONS-AND-RECIPES.md) for working placement
shapes. For projects that combine cooked Unreal content with RuneSchema
registration, see [Create new PAK content](PAK-CONTENT-GUIDE.md).

## All loader mock-ups

Each [loader walkthrough](LOADER-WALKTHROUGHS.md) now gives a numbered workflow,
JSONC mock-up, required asset/identity replacements and links to its consumers.
Start there instead of installing an unrelated integration fixture.

## Diagnostics

Use the [Live Log and troubleshooting guide](TROUBLESHOOTING.md) and
[diagnostic code reference](ERROR-CODES.md) to investigate the affected subsystem.
Keep diagnostic collection scoped and review logs before sharing them.
