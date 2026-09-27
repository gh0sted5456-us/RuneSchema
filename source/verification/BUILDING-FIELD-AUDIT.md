# Building field audit

This audit records the UE 5.6.1 building surface used by RuneSchema's
`/buildings` loader. It was checked against the bundled UE4SS
`Mappings.usmap` and the RSDW FModel exports.

## Sources

- `/Script/Dominion.BuildingPieceData`
- `/Script/Dominion.PlacementProfile`
- `/Script/Dominion.StabilityProfile`
- `/Script/Dominion.BuildingPieceDerivedData`
- `/Script/Dominion.BuildingShelterComponent`
- `/Script/Dominion.HealthComponent`
- `/Script/Dominion.BuildingSnapComponent`
- `Content/Gameplay/BaseBuilding_New/DT_PlacementProfile.json`
- `Content/Gameplay/BaseBuilding_New/DT_StabilityProfile.json`
- station actor exports under `Content/Gameplay/World/Stations`

Use `usmap_type_report.py` to repeat the mapped-type inspection after a game
update. Runtime reflection remains authoritative; RuneSchema rejects a field
when the selected storefront/game build does not expose the expected layout.

## Exposed placement profile

All 23 mapped fields are exposed: user height/rotation, snap-only placement,
forced plug rotation, shared plugs, rotation offset, defined-surface and
physical-surface restrictions, surface normals, ground-only placement, hit
surface rotation, projection normal, non-building offset, magnetizing and
overlap multipliers, building overlap, overlap tag exceptions, region block
query, forced blocker overlap, and snapping-mode override.

Native snapping modes are `Basic`, `Advanced`, and `Free`. The only physical
surface used by the shipped table is currently `SurfaceType21`, but the loader
accepts any enum name recognized by the running game.

## Exposed stability profile

All four mapped fields are exposed: `MaxStability`, `MinStability`,
`VerticalLoss`, and `HorizontalLoss`. An existing native row may be selected
as a template. RuneSchema writes numeric changes to a target-owned copied row;
it never edits the shared native row.

## Exposed derived placement data

The placement-related mapped fields are `PlacementZOffset`,
`PhysicalSurfaceExtentNeg`, and `PhysicalSurfaceExtentPos`. RuneSchema clones
`BuildingPieceDerivedData` for `$Clone` entries before applying them.

## Exposed shelter, health, and snapping components

All 14 mapped shelter fields are exposed: interaction requirement, placement
check, non-building actor inclusion, four requirement texts, roof and shelter
rays, two trace exclusion filters, sweep thickness/distance, and validity
percentage.

Health exposes authored defaults (`MaxHealth`, `bCanDie`), not live replicated
health. Snapping exposes socket plug generation and both mapped snap radii.

These component settings live on the actor template. A `$Clone` must use a
private cooked `BuildableActor` before RuneSchema will write them. An `Asset`
definition is allowed to intentionally override the existing actor.

## Intentionally protected fields

RuneSchema owns building persistence identity, internal name, catalogue index,
and the top-level requirements transaction. Runtime-only actor state,
delegates, object caches, current health, preview flags, live stability, and
replication-manager state are not authoring fields.
