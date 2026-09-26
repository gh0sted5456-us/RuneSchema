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
and [BUILDING-CLONING-FMODEL-AUDIT.md](../BUILDING-CLONING.md).

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

### FAQ-BUILDINGS-003 — What happens if I omit AddTo on a clone? {#faq-buildings-003}

The clone inherits the catalogue placements that contain its source. Use
`AddTo` when you want an explicit collection or page.

### FAQ-BUILDINGS-004 — Does Requirements append to the source cost? {#faq-buildings-004}

No. `Requirements` replaces the complete material-cost list.

### FAQ-BUILDINGS-005 — Can BuildableActor point at any Blueprint? {#faq-buildings-005}

No. Use a cooked compatible child of the game's base building actor and verify
placement, collision, save/reload, and client presentation.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
