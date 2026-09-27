# Buildings loader

**Folder:** `RuneSchema/mods/<ModName>/buildings/`

BuildingPieceData registration, cloning, costs, build-menu placement, placement
rules, stability, shelter checks, snapping, health, and station behavior.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/buildings` to register an existing `BuildingPieceData` asset or clone one
as a separate build-menu entry.

```jsonc
{
  "MyWall": {
    "$Clone": "/Game/Gameplay/Building/Data/BUILDING_Source.BUILDING_Source",
    "Properties": {
      "BuildableActor": "/Game/MyMod/Buildings/BP_MyWall.BP_MyWall_C"
    },
    "Requirements": [
      {"ItemData":"/Game/Gameplay/Items/ITEM_Log.ITEM_Log","Amount":4}
    ],
    "Overrides": {
      "Placement": {
        "Profile": "WallPropProfile",
        "bAllowUserRotationModification": true,
        "bCanOnlyBeSnapped": false,
        "bCanOnlyBePlacedOnGround": false,
        "bCanOnlyBePlacedOnDefinedSurface": true,
        "SurfacePlacementNormal": {"X":0,"Y":1,"Z":0},
        "bOverrideSnappingMode": true,
        "SnappingModeOverride": "Advanced"
      },
      "Stability": {
        "Profile": "Tier2_Base",
        "MaxStability": 1800,
        "HorizontalLoss": 1.25
      },
      "DerivedData": {"PlacementZOffset": 10.0},
      "Shelter": {
        "InteractionRequirements": "InteractInShelterOnly",
        "bShelterCheckedOnPlacement": true,
        "SweepRayDistance": 500.0
      },
      "Health": {"MaxHealth": 2500,"bCanDie": true},
      "Snapping": {
        "SnappingRadius": 150.0,
        "SnappingRadiusInBasicSnappingMode": 100.0
      }
    },
    "Unlock": true,
    "AddTo": {"Collection":"Modded Buildings","PageIndex":0}
  }
}
```

Walkthrough:

1. Select a compatible `BuildingPieceData` source.
2. Use `$Clone` for a new identity or `Asset` to register an existing record.
3. Point `BuildableActor` at a cooked child of the game's base building actor.
4. Replace the full requirements list when changing cost.
5. Set `AddTo`, or omit it to inherit pages containing the clone source.
6. Test placement, collision, navigation, save/reload, and deconstruction on
   both host and client.

`PersistenceID`, `InternalName`, piece index, and requirements cannot be hidden
inside `Properties`; RuneSchema owns those fields. For imported assemblies and
static parent objects, see [Base Builder import](../raw/BASE-BUILDER-IMPORT.md)
and [Building cloning](../BUILDING-CLONING.md).

For compatible processing buildings, `Overrides.Processing` can append,
replace, or clear accepted fuels and can set fuel slots, processing rate, and
automatic start. RuneSchema rejects unsupported fields without discarding an
otherwise valid building definition.

## Existing pieces and clones

Both authoring modes accept `Requirements`, `AddTo`, and `Overrides`:

- `Asset` targets the existing cooked `BuildingPieceData`. The change is
  intentionally visible everywhere that piece is used.
- `$Clone` creates a separate RuneSchema identity and build-menu entry.

Placement and stability profiles are rows in shared game tables. RuneSchema
never edits the selected vanilla row in place. It copies the selected row into
a deterministic RuneSchema-owned row, applies the requested fields, and points
only the target building at that row. A clone also receives private transient
`BuildingPieceDerivedData`; this prevents its placement settings from leaking
back into the vanilla source.

Shelter, health, snapping, and world/interact names live on the buildable actor
or its component templates. They cannot be isolated by cloning only the
`BuildingPieceData`. A `$Clone` using any of those overrides must therefore set
`Properties.BuildableActor` to its own cooked actor. An `Asset` override may
intentionally edit the existing actor.

## BuildingPieceData fields

`Properties` writes reflected `BuildingPieceData` fields. The UE 5.6.1 mapping
contains: `DerivedData`, `DisplayName`, `Description`, `Category`,
`DisplayIcon`, `BuildableActor`, `ReducedRefundedMaterials`,
`ShelterRequirementIcon`, `ShelterRequirementText`, `ConstructionXPOnBuild`,
`PieceTag`, `bIgnoreAutomaticUnlocks`, `bOnlyAvailableToHost`,
`BuildingStabilityProfileRowHandle`, `BuildingPieceProxyData`, `BuildXpEvent`,
`PlacementPlayerHooks`, `SelectionPlayerHooks`, `RepresentationCategory`, and
`bShouldBeVisibleFromFarAway`.

RuneSchema owns and rejects direct writes to `PersistenceID`, `InternalName`,
`BuildingPieceDataIndex`, and `Requirements`. Use the top-level `Requirements`
array for cost. Use `Overrides.Stability` rather than manually constructing a
row handle. `BuildableActor` replacement is allowed only on `$Clone`.

## Placement fields

`Overrides.Placement.Profile` may name any existing row in the piece's native
placement table. The shipped rows are `FoundationProfile`, `BaseProfile`,
`RoofProfile`, `PropProfile`, `WallPropProfile`, `FloorPropProfile`,
`DoorWindowProfile`, `WaterProfile`, `CeilingPropProfile`,
`FloorPropNonOverlapProfile`, `FarmingPlotProfile`, `FarmingWellProfile`, and
`FarmingPlotProfileUmbral`.

Every mapped `PlacementProfile` field is exposed:

| Field | Meaning |
|---|---|
| `bAllowUserHeightModification` | Player may change placement height. |
| `bAllowUserRotationModification` | Player may rotate the placement ghost. |
| `bCanOnlyBeSnapped` | Reject free placement. |
| `bForcePlugRotation` | Use the selected snap plug's rotation. |
| `bCanSharePlugWithSamePieces` | Same piece types may share a plug. |
| `SurfaceRotationOffset` | Pitch/yaw/roll added for surface placement. |
| `bCanOnlyBePlacedOnDefinedSurface` | Require the configured surface normal. |
| `SurfacePlacementNormal` | Required surface normal vector. |
| `bCanOnlyBePlacedOnCertainPhysicalSurfaces` | Enable the physical-surface allow list. |
| `AcceptedPhysicalSurfaces` | Physical surface enum names, such as `SurfaceType21`. |
| `bOverrideRotationFromHitSurface` | Align using the hit surface. |
| `bCanOnlyBePlacedOnGround` | Require ground placement. |
| `bOverrideProjectionNormal` | Use `OverrideProjectionNormal`. |
| `OverrideProjectionNormal` | Projection vector. |
| `bShouldOffsetFromNonBuildingSurface` | Apply the game's non-building surface offset. |
| `MagnetizingMultiplier` | Native placement magnet strength multiplier. |
| `OverlappingBoundsMultiplier` | Native overlap bounds multiplier. |
| `bAllowOverlappingWithBuildingPieces` | Permit building-piece overlap. |
| `OverlapExceptionFilter` | Gameplay tags exempted from overlap rejection. |
| `RegionBlockList` | Advanced native `GameplayTagQuery`; copy a known-good FModel value. |
| `bForceBuildingBlockerOverlapDuringPlacement` | Force blocker overlap testing. |
| `bOverrideSnappingMode` | Enable the snapping-mode override. |
| `SnappingModeOverride` | `Basic`, `Advanced`, or `Free`. |

`RequiresFoundation` remains a convenience alias: `false` selects
`PropProfile` when no profile is supplied; `true` requires an explicit profile.
`RequiresRoof` and `RequiresShelter` are compatibility aliases for the shelter
component's interaction requirement.

## Stability and derived placement

`Overrides.Stability` accepts an existing `Profile` and any combination of
`MaxStability`, `MinStability`, `VerticalLoss`, and `HorizontalLoss`. Numeric
changes create a private copied row. Native profiles range from `Prop` and
`Stackable_Prop` through `Tier1_*`, `Tier2_*`, `Tier3_*`, and `FarmPlot`.

`Overrides.DerivedData` exposes `PlacementZOffset`,
`PhysicalSurfaceExtentNeg`, and `PhysicalSurfaceExtentPos`. These are Unreal
centimetres; `10.0` is 10 cm.

## Roof and shelter checks

`Overrides.Shelter.InteractionRequirements` accepts:

- `InteractInShelterOnly`
- `InteractExternallyOnly`
- `InteractAnywhere`

The remaining mapped fields are `bShelterCheckedOnPlacement`,
`bIncludeNonBuildingPartActors`, `RequiresRoofText`, `RequiresShelterText`,
`RequiresNoRoofText`, `RequiresNoShelterText`, `RoofRays`,
`RoofTraceExclusionFilter`, `SweepRayThickness`, `SweepRayDistance`,
`ShelterRays`, `ValidityPercentage`, and `ShelterTraceExclusionFilter`.
Ray entries are `{ "X": number, "Y": number, "Z": number }`; exclusion
filters are arrays of gameplay-tag strings.

## Health and snapping

`Overrides.Health` exposes `MaxHealth` and `bCanDie` from `HealthComponent`.
`Overrides.Snapping` exposes `bUseSocketsForPlugGeneration`, `SnappingRadius`,
and `SnappingRadiusInBasicSnappingMode` from `BuildingSnapComponent`.

## Simple rules

- Register an existing `BuildingPieceData` asset or clone a compatible one for a new build-menu identity.
- Point `BuildableActor` at a cooked child of the game's base building actor.
- Replace the full requirements list when changing cost. Use `AddTo` for explicit menu placement.

## FAQ

### FAQ-BUILDINGS-001 — Can I hide PersistenceID or InternalName inside Properties? {#faq-buildings-001}

No. `PersistenceID`, `InternalName`, piece index, and requirements are owned
fields in the building definition and cannot be hidden inside `Properties`.

### FAQ-BUILDINGS-002 — Should I use an NPC definition for a static building prop? {#faq-buildings-002}

Normally no. Use `/buildings` for buildable content or `/spawns` for supported
static world placement instead of treating architecture as an NPC.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
