# RSDW Base Builder import

Place an exported `rsdwtools.buildings.v1` JSON file in a mod's `buildings`
directory. RuneSchema consumes the export directly. Add this optional top-level
block to translate and rotate the entire local layout into the game world:

```json
"RuneSchemaPlacement": {
  "Location": { "X": 125000, "Y": -42000, "Z": 1800 },
  "Rotation": { "Yaw": 90 },
  "OriginMode": "LocalOrigin",
  "ImportMode": "StaticAssembly",
  "AssemblyId": "my-castle",
  "NativePieceIds": [41, 42, 87],
  "AllowDeconstruction": false,
  "IncludeGhosted": false
}
```

`ImportMode` supports two deliberately different runtime contracts:

- `NativeBuildingPieces` (default) creates ordinary game building actors. They
  retain native building behavior and each piece remains an independent actor.
- `StaticAssembly` creates one transient RuneSchema-owned parent at `Location`
  and groups matching cooked meshes into
  `HierarchicalInstancedStaticMeshComponent` children. The imported local
  transforms remain relative to that parent, so moving or rotating the center
  preserves the complete layout.

`NativePieceIds` enables hybrid imports in `StaticAssembly` mode. Listed
Base Builder `piece_id` values remain native actors for doors, crafting
stations, storage, or other pieces that need gameplay logic; every unlisted
piece becomes part of the efficient static shell. If a native selection or a
static mesh cannot resolve, RuneSchema reports and skips only that affected
piece.

`OriginMode` supports:

- `LocalOrigin`: Base Builder `(0,0,0)` is placed at `Location`.
- `BoundsCenter`: the center of the exported piece bounds is placed at
  `Location`.
- `AnchorPiece`: the export's `anchor_piece_id` is placed at `Location`.

The layout yaw rotates every local XY offset around the selected origin and is
also added to each piece's local yaw. Piece pitch, roll, scale, stable
`piece_id`, and `piece_data_name` are preserved. Ghosted pieces are excluded by
default. Invalid or missing assets reject only the affected piece and are
reported with the `buildings` loader tag.

Static assemblies use each cooked StaticMesh's authored `BodySetup` and a
blocking collision profile. RuneSchema never mutates shared collision assets.
Floors, walls, roofs, stairs, and door openings therefore remain walkable and
enterable when the source cooked meshes contain appropriate collision. Static
pieces provide geometry and collision only; use `NativePieceIds` for anything
that must open, store items, craft, emit native effects, or participate in the
game's building system. Assemblies are reconstructed deterministically on the
server and each client from the same installed mod definition; HISM instance
arrays are not trusted to replicate themselves.

Base Builder `items` and loose `actors` are not imported as building pieces.
RuneSchema reports and skips them because they require different native
persistence contracts.
