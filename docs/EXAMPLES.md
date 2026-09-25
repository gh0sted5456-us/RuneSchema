# Example library

The documentation bundle includes working and diagnostic examples for the current RuneSchema authoring model. Copy the smallest relevant example and change only the identities, paths, and values required by the mod.

## Character customization

| Example | Purpose |
|---|---|
| [`hair-menu.json`](examples/CharacterCustomization/assets/domains/character_customization/hair-menu.json) | Adds a character-menu option through `/assets` |
| [`unlock-vanilla-beards.json`](examples/CharacterCustomization/assets/domains/character_customization/unlock-vanilla-beards.json) | Uses `$MergeWhere` to change a group of existing menu entries |
| [`hair.json`](examples/CharacterCustomization/character_customization/hair.json) | Character-customization definition example |

## Registry patching

| Example | Purpose |
|---|---|
| [`add-owned-row.json`](examples/RegistryPatch/patches/add-owned-row.json) | Adds a RuneSchema-owned DataTable row through the registry patch profile |

## Building clones

| Example | Purpose |
|---|---|
| [`10-CookedProps.jsonc`](examples/BuildingClone/buildings/10-CookedProps.jsonc) | Building clone / cooked-prop authoring example |

## Integrated RSv16 examples

`examples/RSv16/ExampleMods/RuneSchema2VendorTest/` demonstrates multiple loaders working together, including NPCs, vendors, dialogue, quests, events, lore, spawns, raw data, and assets.

Useful entry points:

- [`CowTrader`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/npc/00-CowTrader.jsonc) — NPC definition.
- [`GraniteArmory`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/vendors/20-GraniteArmory.jsonc) — vendor definition.
- [`GraniteStory`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/dialogue/20-GraniteStory.jsonc) — dialogue definition.
- [`GraniteGoblinPatrol`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/quests/60-GraniteGoblinPatrol.jsonc) — quest definition.
- [`MortimerEncounters`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/events/60-MortimerEncounters.json) — event definition.
- [`MortimerZombies`](examples/RSv16/ExampleMods/RuneSchema2VendorTest/spawns/60-MortimerZombies.json) — spawn definition.

## Diagnostic presets and trace profiles

`examples/RSv16/DiagnosticPresets/` and `examples/RSv16/TraceProfiles/` contain targeted diagnostics for equipment, vendors, weather, effects, and utility magic.

These files are references. They are not installed into the public runtime automatically.
