# RuneSchema 0.7.5.6 building cloning and cooked-asset contract

This contract is based on the RSDWArchive Unreal 5.6.1 FModel exports for the
default build catalogue, `BuildingPieceData`, its cooked derived data, the
base-building Blueprint hierarchy, plan items, and progression tables. It is
also checked against the RSDW Base Builder catalogue/export format.

## What the game separates

A build-menu entry is a `BuildingPieceData`, not an actor or mesh. The audited
Sawhorse entry contains these independent fields:

- `DisplayName` and `DisplayIcon` for menu presentation.
- `BuildableActor` for the class spawned when the piece is built.
- `Requirements` for its complete material cost.
- `PieceTag`, `BuildingStabilityProfileRowHandle`, and `BuildXpEvent` for build
  rules and rewards.
- `RepresentationCategory` and `DerivedData` for runtime representation,
  snapping, bounds, sound, and placement data.
- `PersistenceID` for save and network registry identity.

The default catalogue then owns `Pages[]`, each page owns labeled
`Collection[]` groups, and each group owns soft references to
`BuildingPieceData`. The audited source locations are:

| Source | Page | Index | Collection |
| --- | --- | ---: | --- |
| Sawhorse | Decoration | 3 | General |
| Laying Book | Decoration | 3 | Literature |

RuneSchema scans the live catalogue rather than hard-coding these labels or
indices. This survives catalogue changes and also preserves every source
membership if a source appears in more than one collection.

## Actor and derived-data findings

Both audited actors derive from `BP_BaseBuilding_PropBase_C`, which derives
from `BP_BaseBuilding_BaseActor_C`. Their cooked defaults provide the native
building binding, root, mesh, collider, snap, interaction, health, and damage
components. A replacement cooked Blueprint must therefore derive from
`BP_BaseBuilding_BaseActor` (normally `BP_BaseBuilding_PropBase`) and retain a
usable `BuildingPieceData`, `BuildingData`, `BuildingPiece`, or
`BuildingPieceDataIndex` binding.

The source Sawhorse is `Lightweight`. Its `BuildingPieceDerivedData` embeds the
vanilla Sawhorse static mesh in `EntityRepresentation`, as well as snap plugs,
bounds, placement profile, and sounds. Changing only `BuildableActor` while
leaving `Lightweight` selected can consequently render the vanilla mesh to a
remote client. When a clone overrides `BuildableActor` and does not explicitly
override `RepresentationCategory`, RuneSchema selects `ManagedActor`. This
causes the validated cooked actor to be the represented object on host and
clients while retaining the source placement/stability rules.

For a differently sized prop, author the cooked Blueprint's collider and local
bounds for the replacement. Cloning a source with a similar footprint gives
the safest inherited snap/placement behavior. Advanced authors may supply
their own cooked `DerivedData`, but it must be generated from the replacement
Blueprint and tested on a dedicated server and a remote client.

## JSON contract

`$Clone` creates a new transient `BuildingPieceData`. It never replaces the
source. RuneSchema assigns a stable `RuneSchema:<mod>:<key>` identity, registers
it in the building subsystem/network maps, and appends it to the menu.

When `AddTo` is omitted, the clone is appended to every collection containing
the source. An explicit object or array opts out of inheritance and places the
clone in the requested collection(s).

`Requirements` is a complete replacement cost, not an append operation. Every
`ItemData` must resolve and every `Amount` must be a positive integer before
the clone is committed.

`Properties.BuildableActor` must be a full cooked generated-class path ending
in `_C`. The class must load, be concrete, derive from
`BP_BaseBuilding_BaseActor`, and expose a native building-data binding. The
same pak and JSON must be installed on the server and every client.

RuneSchema refuses these managed fields inside `Properties`:

- `PersistenceID`
- `InternalName`
- `BuildingPieceDataIndex`
- `Requirements` (use the top-level array)

Example:

```jsonc
{
  "quest_board": {
    "$Clone": "/Game/Gameplay/BaseBuilding_New/BuildingPieces/Decorations/General/DA_BaseBuilding_Decoration_General_SawHorse.DA_BaseBuilding_Decoration_General_SawHorse",
    "Properties": {
      "BuildableActor": "/Coinage/Buildings/Actors/BP_Buildable_QuestBoard.BP_Buildable_QuestBoard_C",
      "DisplayName": "Quest Board"
    },
    "Requirements": [
      {
        "ItemData": "/Game/RuneSchema/Currency/Items/rs_currency_copper.rs_currency_copper",
        "Amount": 100
      }
    ],
    "Unlock": true
  }
}
```

That entry appears beside Sawhorse in Decoration / General. To use a custom
category instead:

```jsonc
"AddTo": { "Collection": "Currency", "PageIndex": 0 }
```

Multiple explicit placements are accepted as an array.

## Load and failure behavior

The loader validates source type, replacement actor, reflected writes,
requirements, stability, identity, and catalogue placement in order. A failed
clone is not inserted into the RuneSchema building map or network registry;
its transient root is released. Errors are tagged to the individual building,
and unrelated definitions continue.

The original source entry is never removed. `Unlock: true` adds the clone to
the current unlock set and the session-only native set. It does not rewrite a
vanilla plan item. Authors who want progression-controlled unlocks should set
`Unlock: false` and drive the cloned entry through a RuneSchema quest/event or
another explicitly tested progression rule.

## Multiplayer acceptance test

1. Install the same pak, RuneSchema mod folder, and JSON on server and client.
2. Confirm the log reports the clone with zero building errors.
3. Open the source menu category and verify both original and cloned entries.
4. Verify the displayed replacement cost and insufficient-material behavior.
5. Place a ghost, rotate/snap it, build it, reconnect, and restart the server.
6. Verify host and remote client see the cooked replacement actor—not the
   source mesh—and that collision, health, damage, interaction, and deletion
   policy behave as authored.
7. Remove/disable the owning mod only after removing its placed world content,
   unless the building retirement manifest has been tested for that release.

## FModel evidence used

- `DA_BuildPieceCatalogue_Default`
- `DA_BaseBuilding_Decoration_General_SawHorse`
- `DA_BaseBuilding_Decoration_General_SawHorse_DerivedData`
- `BP_BaseBuilding_Decoration_General_SawHorse`
- `BUILDPIECE_DA_BaseBuilding_Decoration_Literature_Book_Tome_Generic_Laying`
- `BP_BaseBuilding_Decoration_Literature_Book_Tome_Generic_Laying`
- `BP_BaseBuilding_PropBase`
- `DT_Progression_BuildPieces` and `DT_Progression_Decorations`
- `DA_Consumable_Plan_*` records with `BuildingPieceToUnlock`
