# Building cloning

Use this page for RuneSchema building clones and cooked replacement actors.

## What a building entry is

A build-menu entry is a `BuildingPieceData` asset. It is separate from the
actor or mesh that appears after placement.

Important fields include:

- display name and icon;
- `BuildableActor`;
- material requirements;
- placement/stability data;
- representation mode;
- persistence identity.

RuneSchema scans the live build catalogue for menu placement instead of
hard-coding vanilla page names.

## Clone rules

`$Clone` creates a new RuneSchema-owned building entry. It does not replace
the source.

When `AddTo` is omitted, the clone inherits the source's catalogue placement.
Use `AddTo` when you want an explicit collection or page.

`Requirements` replaces the complete source cost. Every item must resolve and
every amount must be a positive integer.

Do not place these fields inside `Properties`:

- `PersistenceID`
- `InternalName`
- `BuildingPieceDataIndex`
- `Requirements`

RuneSchema owns those fields.

## Replacement actors

A custom `BuildableActor` must be a cooked generated class that derives from
the Dragonwilds base building actor and retains a usable native building-data
binding.

Use a full generated-class path ending in `_C`.

When a clone replaces the source actor and does not specify another
representation mode, RuneSchema uses a managed-actor representation so clients
render the replacement actor instead of the source's lightweight mesh.

For differently sized props, author collision and local bounds in the cooked
Blueprint. A source with a similar footprint is the safest clone base.

## Example

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

For explicit placement:

```jsonc
"AddTo": {
  "Collection": "Currency",
  "PageIndex": 0
}
```

`AddTo` may also be an array when the clone belongs in more than one place.

## Unlocks

`Unlock: true` adds the clone to the current unlock state. It does not rewrite
a vanilla plan item.

For progression-controlled buildings, use `Unlock: false` and grant access
through a tested RuneSchema quest, event, or other progression rule.

## FAQ

### FAQ-BUILDCLONE-001 — Does $Clone replace the vanilla building source? {#faq-buildclone-001}

No. It creates a separate RuneSchema-owned building entry and leaves the source
in place.

### FAQ-BUILDCLONE-002 — What happens if AddTo is omitted? {#faq-buildclone-002}

The clone inherits the source's catalogue placement.

### FAQ-BUILDCLONE-003 — Does Requirements add to the source material cost? {#faq-buildclone-003}

No. It replaces the complete requirements list.

### FAQ-BUILDCLONE-004 — What kind of path should BuildableActor use? {#faq-buildclone-004}

Use the full cooked generated-class path for a compatible building actor,
ending in `_C`.

### FAQ-BUILDCLONE-005 — Does Unlock:true rewrite a vanilla plan item? {#faq-buildclone-005}

No. It adds the clone to the current unlock state; it does not rewrite a
vanilla plan item.

### FAQ-BUILDCLONE-006 — Do server and clients need the same cooked building assets? {#faq-buildclone-006}

Yes. Install the same cooked replacement assets and RuneSchema definitions on
the server and clients that participate in the multiplayer test.


## Multiplayer test

Install the same cooked assets and RuneSchema definitions on the server and
clients, then verify:

1. the source and clone both appear where expected;
2. the replacement cost is correct;
3. the placement ghost, rotation, snapping, and collision work;
4. the built actor is correct on host and remote clients;
5. save/reload and reconnect keep the same building identity;
6. health, damage, interaction, and deconstruction behave as authored.

For RSDW Base Builder imports and static assemblies, see
[Base Builder Import](raw/BASE-BUILDER-IMPORT.md).
