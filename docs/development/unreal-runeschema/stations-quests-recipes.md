# Coinage authoring guide

This guide records the working pattern used by Coinage to add independent
Dragonwilds building stations, RuneSchema recipes, and quest chains. It also
documents the runtime clone-of-clone failure encountered by the gold-trimmed
armour recipe.

## Architecture

Coinage is split into two cooperating layers:

1. **Cooked Unreal content** supplies meshes, materials, collision, buildable
   actors, and `BuildingPieceData` assets under `/Game/Mods/Coinage`.
2. **RuneSchema definitions** register catalogue rows, recipes, items, NPC or
   prop interactions, quests, dialogue, unlocks, and build-menu exposure.

The cooked layer provides real Unreal objects. RuneSchema connects those
objects to the game's registries. A JSON definition cannot turn a static mesh
into a functional station by itself.

## Building an independent crafting station

An independent station may inherit the game's proven station Blueprint, but it
must not retain the vanilla station's data-table row. Blueprint inheritance is
implementation reuse; the component's row handle is the station's gameplay
identity.

The Crimson Ledger follows this pattern:

1. Mirror the required game Blueprint and native Dominion types into the
   authoring project at their exact `/Game/Gameplay/...` and `/Script/Dominion`
   paths.
2. Create a Coinage-owned child or derived actor under
   `/Game/Mods/Coinage/Buildings/Actors`.
3. Keep the vanilla crafting interaction and `CraftingStationComponent`.
4. Change that component's station row from the vanilla row to the private
   `CoinageLedger` row in `DT_CraftingStationsDataTable`.
5. Give the actor Coinage presentation, mesh, collision, and placement values.
6. Create a standalone Coinage `BuildingPieceData` asset whose
   `BuildableActor` points to the Coinage actor.
7. Give the building piece a unique `PersistenceID` and `InternalName`.
8. Register and expose the cooked building piece through RuneSchema.

The private row is authored in:

`Distribution/RuneSchema/mods/Currency/raw/20-CoinageLedgerStation.jsonc`

Recipes target that row instead of `CraftingTable` or `JewelersBench`:

```jsonc
"AddTo": [{
  "Table": "DT_CraftingStationsDataTable",
  "Row": "CoinageLedger",
  "Category": "Convert"
}]
```

If the actor still opens the vanilla menu contents, inspect the station
component's row handle first. A renamed child that still points at the vanilla
row is only a visual reskin.

## Building an independent processing station

The Salvage Assayer uses the corresponding processing-station pattern:

1. Derive a Coinage actor from the game's processing-station master.
2. Preserve the native interaction and `ProcessingStationComponent`.
3. Point the component to the private `CoinageSalvageAssayer` row in
   `DT_ProcessingStationDataTable`.
4. Author accepted fuel, resource slots, fuel slots, rate, and start behavior in
   that private row.
5. Point a standalone Coinage `BuildingPieceData` asset at the actor.
6. Register the cooked building piece in RuneSchema's building catalogue.

The working Assayer row is in:

`Distribution/RuneSchema/mods/Currency/raw/21-CoinageSalvageAssayer.jsonc`

Important settings:

- `MaxResourceSlots` must accommodate the recipe's distinct consumed inputs.
  A steel item plus Gold Coins requires two resource slots.
- `MaxFuelSlots` is separate from resource slots.
- `AcceptedFuels` controls the fuel lane. Wild Anima being accepted as fuel does
  not replace Gold Coins as a recipe input.
- Every processing recipe must target the private processing row's `Recipes`
  array.

```jsonc
"AddTo": [{
  "Table": "DT_ProcessingStationDataTable",
  "Row": "CoinageSalvageAssayer",
  "Array": "Recipes"
}]
```

If the Assayer says “Furnace,” opens the Furnace inventory, or overlays Furnace
visuals, its component, inherited defaults, or row handle still points at the
vanilla Furnace contract.

## Building-piece registration

Cooked standalone building pieces are exposed with `Asset`, not `$Clone`:

```jsonc
"salvage_assayer": {
  "Asset": "/Game/Mods/Coinage/Data/Buildings/BUILDPIECE_Coinage_SalvageAssayer.BUILDPIECE_Coinage_SalvageAssayer",
  "Unlock": true,
  "AddTo": { "Collection": "Currency", "PageIndex": 0 }
}
```

Use `$Clone` only when RuneSchema is expected to create a new runtime object
from another building definition. Do not combine `Asset` and `$Clone` for one
entry. A cooked `BuildingPieceData` still needs valid identity fields in the
asset itself; build-catalogue protection rejects pieces missing both
`PersistenceID` and `InternalName`.

The final package must contain both the `BuildingPieceData` and every referenced
Coinage actor, mesh, material, and texture. Appearing in the build menu proves
registration, not that the spawned actor and all of its dependencies can load.

## Placement and collision

Placement behavior comes from the building piece's placement profile and actor
configuration.

- Use a floor or surface profile for tabletop props that intentionally require
  a supporting surface.
- Use a terrain-capable prop profile when a board or station should place on
  the ground without a foundation.
- Keep collision simple. Author primitive/simple collision on the mesh or use a
  small actor collision component around the interactable volume.
- Verify that build-preview scale, placed-actor scale, interaction range, and
  collision all agree. Fixing only the visible mesh scale can leave an oversized
  collision or interaction target.
- Snap points are explicit building data; a visible mesh does not acquire them
  automatically.

## Recipe authoring

A recipe requires three independent decisions:

1. Which station row receives it.
2. Which items and counts it consumes.
3. Which stable item object it creates.

Crafting-station recipes use `DT_CraftingStationsDataTable`. Processing recipes
use `DT_ProcessingStationDataTable`. Do not register one recipe with both unless
the design explicitly calls for both stations.

Use full object paths, including the object name after the period. Keep those
paths identical in assets, recipes, quests, rewards, and loot tables.

For processing recipes, `MinProcessingTime` and `MaxProcessingTime` control the
process duration. Fuel remains governed by the station row, not by adding the
fuel item to `ItemsConsumed`.

## Runtime item identities

A new item requires a unique and stable:

- object path;
- `PersistenceID`;
- `InternalName`.

Never reuse another mod's identity. Changing only the display name does not
create an independent item.

The safe gold-trimmed armour pattern is:

1. Clone the matching vanilla steel `ItemData` directly.
2. Assign a Coinage path under `/Game/RuneSchema/Currency/Items`.
3. Assign Coinage-owned identity fields.
4. Override the icon and wearable mesh-data references with BlackG's cooked
   visual assets.
5. Make recipes and quests reference the Coinage runtime path.

This preserves the vanilla item's native class and serialized field layout
while borrowing only cooked presentation dependencies.

## Why clone-of-clone is unsafe

RuneSchema `$Clone` objects are runtime-created `UObject` instances. They are
not equivalent to source packages cooked by Unreal.

The failed design cloned BlackG's runtime-created helmet item into another
runtime-created Coinage item. Crafting completed only when the game attempted
to materialize, serialize, replicate, or save the output. That delayed access
made the recipe appear valid until the Assayer produced the item, at which
point the process crashed.

Avoid this graph:

```text
vanilla cooked ItemData
  -> BlackG runtime $Clone
     -> Coinage runtime $Clone
```

Use this graph instead:

```text
vanilla cooked ItemData
  -> Coinage runtime $Clone
     -> BlackG cooked icon and mesh-data references
```

General rules:

- Clone from a stable cooked game asset whenever possible.
- Treat another mod's runtime clone as an endpoint, not a template.
- Borrow cooked visual references explicitly rather than inheriting an entire
  runtime-created item.
- Ensure dependency mods mount before RuneSchema resolves those visual paths.
- A successful JSON load or menu preview does not prove the output is safe.
  Test crafting completion, inventory transfer, dropping, equipping, saving,
  reloading, and multiplayer replication.

## Quest authoring

Each quest file contains one quest definition with a unique `Id` and
`PersistenceID`. Follow-on quests use `Prerequisites`:

```jsonc
"Prerequisites": ["coinage_mintmasters_trial"]
```

Use `Acquire` for progress driven by item-added events. `Acquire` is not
retroactive: an item already in the player's inventory before the quest or
stage begins does not automatically count. The player must acquire it after the
objective is active.

This affects quest-chain design. Coinage's helmet quest proves the helmet in
its own stage. The following full-set quest asks for the body and legs only,
because requiring the helmet again would force the player to craft a duplicate
or drop and reacquire the existing helmet.

`Collect` is a turn-in-oriented objective, not a substitute for live acquisition
tracking. Do not expect it to advance immediately when inventory changes.

For a board-style quest giver:

- use an NPC definition with `"Type": "Prop"`;
- provide a cooked mesh, collision, location, and rotation;
- connect it to a dialogue definition with `DialogueID`;
- use dialogue quest actions to `Accept`, `TurnIn`, or `Abandon` quests;
- gate turn-in choices with `States: ["Active"]` and
  `ObjectivesComplete: true`.

Quest rewards reference the same stable item paths used by recipes. The final
Coinage armour quest awards 99 Gold Coins through its `Reward` object.

## Dialogue and chained quests

Dialogue does not replace quest prerequisites. Use both:

- `Prerequisites` enforces quest ordering in the quest service.
- `RequirementUnlock` controls when dialogue choices are available.

Keep acceptance and turn-in as separate choices. A typical turn-in gate is:

```jsonc
"RequirementUnlock": {
  "Quest": {
    "Id": "coinage_gilded_helm",
    "States": ["Active"],
    "ObjectivesComplete": true
  }
},
"Quest": {"Id": "coinage_gilded_helm", "Action": "TurnIn"}
```

Test every dialogue state: not started, active/incomplete, active/complete, and
completed. Also test reloading a save in each state.

## Cooking and packaging

Cook `RSDragonwilds.uproject` so output lands beneath:

`Saved/Cooked/Windows/RSDragonwilds/Content/Mods/Coinage/`

Package only `/RSDragonwilds/Content/Mods/Coinage` into:

- `Coinage_P.pak`
- `Coinage_P.ucas`
- `Coinage_P.utoc`

Do not package the mirrored `/Game/Gameplay` authoring dependencies. The
shipping game supplies those packages at runtime. Do not package global or game
shader archives; overriding the shipping shader libraries can crash startup.

Place the trio beneath the Currency mod's `paks/Coinage` folder and keep the
RuneSchema definitions beside it. See `Build/PACKAGING.md` for the complete
package boundary.

## Verification checklist

Before release:

- All JSONC files parse.
- Every identity is unique and stable.
- Every object path includes package and object names.
- Building entries use exactly one of `Asset` or `$Clone`.
- Cooked building pieces resolve as `BuildingPieceData`.
- Station actors point at their private Coinage table rows.
- Recipes are present only in the intended station.
- Resource and fuel slot counts fit each processing recipe.
- The build preview and placed actor have correct scale, terrain rules,
  collision, snap points, and interaction text.
- Crafted outputs can enter inventory, drop into the world, equip, save, and
  reload without a crash.
- Quest objectives advance only after the quest stage is active.
- Follow-on quests cannot complete out of order.
- Rewards use registered item paths.
- The final archive excludes mirrored game content and shader archives.

## Relevant Coinage files

- `Distribution/RuneSchema/mods/Currency/buildings/20-CurrencyProps.jsonc`
- `Distribution/RuneSchema/mods/Currency/raw/20-CoinageLedgerStation.jsonc`
- `Distribution/RuneSchema/mods/Currency/raw/21-CoinageSalvageAssayer.jsonc`
- `Distribution/RuneSchema/mods/Currency/recipes/`
- `Distribution/RuneSchema/mods/Currency/assets/10-GoldTrimArmour.jsonc`
- `Distribution/RuneSchema/mods/Currency/quests/`
- `Distribution/RuneSchema/mods/Currency/dialogue/10-Mintmaster.jsonc`
- `Distribution/RuneSchema/mods/Currency/npc/10-Mintmaster.jsonc`
- `Build/PACKAGING.md`
- `Research/AUTHORING_EVIDENCE.md`
