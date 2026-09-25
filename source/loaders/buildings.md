# Buildings loader

**Folder:** `RuneSchema/mods/<ModName>/buildings/`

BuildingPieceData registration, cloning, costs, and build-menu placement.

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
static parent objects, see [raw/BASE-BUILDER-IMPORT.md](../raw/BASE-BUILDER-IMPORT.md)
and [BUILDING-CLONING-FMODEL-AUDIT.md](../BUILDING-CLONING-FMODEL-AUDIT.md).

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
