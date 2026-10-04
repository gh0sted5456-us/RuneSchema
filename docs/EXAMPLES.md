# Example library

Use the smallest example that matches the problem. Examples are references, not
files installed by the public runtime.

## Override vanilla data

### Cape stat overrides

The Capes examples patch existing rows in `DT_WearableEquipment`. They are a
good reference for small, independent `/raw` overrides.

- [Attack cape override](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/Capes/raw/DT_ITEM_Cape_Attack_override.json)
- [Trimmed Magic skillcape override](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/Capes/raw/DT_ITEM_Cape_Trimmed_Skillcape_Magic_override.json)

Use this pattern when the row already exists and only a few fields change.

## Patch an existing Blueprint or widget

### Character-creation labels

[Fixed Menu](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/FixedMenu/blueprints/character_creation_text.jsonc)
changes labels on the existing character-creation widget.

It demonstrates a focused `/blueprints` patch without creating a new
Blueprint class.

### Live widget behavior

[Restore Appearance](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RestoreAppearance/blueprints/10-RestoreAppearance.jsonc)
shows `$RuntimeWidget` changing live CommonUI widget state and binding an
existing button event to an existing native menu function.

## Character customization

### Current direct authoring

- [Add a hair menu option](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/CharacterCustomization/assets/domains/character_customization/hair-menu.json)
- [Unlock vanilla beards with `$MergeWhere`](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/CharacterCustomization/assets/domains/character_customization/unlock-vanilla-beards.json)
- [Character customization definition](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/CharacterCustomization/character_customization/hair.json)

### Registry-patch v1 form

[Unlock Vanilla Beards registry-patch sample](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/CharacterCustomization/registry-patch-v1/10-UnlockVanillaBeards.sample.jsonc)
shows the older transaction envelope used by MoreHair.

The sample is intentionally short. The original mod repeats the same merge
operation for the remaining beard rows.

Use the direct `DA_` form for new work unless the registry-patch transaction
model is specifically needed.

## Multi-loader quest and event flow

### Great Tree

The Great Tree sample shows several loaders working as one feature:

1. `/assets` creates quest items and currency.
2. `/spawns` defines event-only enemies.
3. `/events` arranges the encounter waves.
4. `/quests` ties kill and collect objectives to that encounter.
5. `/raw` adds boss loot.
6. `/vendors` spends the custom currency.
7. `/npc` exposes a world object linked to lore.

Files:

- [Items](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/assets/85-GreatTreeItems.json)
- [Quest](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/quests/87-GreatTreeStory.json)
- [Tribe trial event](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/events/85-GreatTree-TribeTrial.json)
- [Event-only spawn templates](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/spawns/87-GreatTree-TribeTrial.json)
- [Boss loot](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/raw/85-GreatTreeLoot.json)
- [Crag vendor](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/vendors/85-Crag.json)
- [Great Tree world object](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/GreatTree/npc/85-GreatTree.json)

This is a better starting point for a connected feature than treating each
loader in isolation.

## Building clones

- [Cooked prop and building example](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/BuildingClone/buildings/10-CookedProps.jsonc)
- [Processing-station override example](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchemaBuildingOverrideExample/buildings/10-CoinageStations.jsonc)

The first example covers a new building or prop. The station example shows
cost replacement, accepted fuel changes, fuel slots, processing speed, and
automatic-start behavior.

## Recipe unlock items

[Dawnveil recipe unlocker](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchemaRecipeUnlockConsumable/assets/10-DawnveilRecipeUnlocker.jsonc)
clones an existing consumable and assigns recipes for it to unlock. Recipe
names may point to a RuneSchema recipe ID or a compatible cooked recipe path.

## Crafting and processing placement

- Use `Category` for a named crafting or merchant category.
- Use `Array: "Recipes"` for a timed processing station.
- Use an exact `DataTable` path for a custom cooked station table.
- Reference an `/assets` clone by its RuneSchema object path. RuneSchema checks
  that the clone is fully registered before accepting a timed output.

See [Create stations and recipes](STATIONS-AND-RECIPES.md) for working placement
shapes. For projects that combine cooked Unreal content with RuneSchema
registration, see [Create new PAK content](PAK-CONTENT-GUIDE.md).

## Integrated RSv16 test mod

`examples/RSv16/ExampleMods/RuneSchema2VendorTest/` remains the broad
integration example.

Useful entry points:

- [Cow Trader NPC](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/npc/00-CowTrader.jsonc)
- [Granite Armory vendor](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/vendors/20-GraniteArmory.jsonc)
- [Granite dialogue](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/dialogue/20-GraniteStory.jsonc)
- [Granite Goblin Patrol quest](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/quests/60-GraniteGoblinPatrol.jsonc)
- [Mortimer encounter](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/events/60-MortimerEncounters.json)
- [Mortimer zombie spawns](https://github.com/gh0sted5456-us/RuneSchema/blob/main/source/examples/RSv16/ExampleMods/RuneSchema2VendorTest/spawns/60-MortimerZombies.json)

## Diagnostics

`examples/RSv16/DiagnosticPresets/` and `examples/RSv16/TraceProfiles/`
contain targeted diagnostics for equipment, vendors, weather, effects, and
utility magic.
