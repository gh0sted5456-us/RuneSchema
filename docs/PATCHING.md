# Field patches and blueprint appearance

## Field patches

Blueprints now accept the same outer identity envelope used by other loaders:

```json
{
  "$Patch": "BP_FellableTree_Ash_C",
  "$Target": {
    "ItemDropOnSplitComponent": {
      "ItemsToDrop": {
        "$Patch": [
          {
            "$Match": {
              "ItemDataClass": "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash"
            },
            "$Target": { "MinToDrop": 3, "MaxToDrop": 5 }
          }
        ]
      }
    }
  }
}
```

Outer `$Patch` identifies a blueprint class (short name ending `_C`, full
`/Game/...Asset.Asset_C` path, or `/Game/.../Asset` package path). `$Target`
contains only the fields to assign. Patches are applied after ordinary blueprint
rules, in patch load order, including when short and full-path rules overlap.
An outer document can be one envelope, an array of envelopes, or named envelopes
inside the existing blueprint document map. Blueprint identity matching is exact.

The inner `$Patch` edits entries of an existing reflected struct array. Use
`$Match` with a non-empty object of fields, or `$Index` with a zero-based index.
Each edit has a non-empty `$Target` field object. Match fields use equality and
all supplied fields must match. Reference fields use full canonical object-path
strings, rather than FModel numeric export suffixes such as `.0`. Prefer stable
item references over quantities that the patch itself changes.

Exactly one entry must match. Missing, ambiguous or out-of-range selectors fail
without appending or clearing entries. Unknown fields inside an array edit are
rejected. All edits for one array are staged using a reflected deep copy and
committed together. This is array-level atomicity, not a transaction across an
entire blueprint or table. Unmentioned fields and other array entries survive.
Nested struct edits and nested struct-array patches are supported. `$Match`
does not match array fields or traverse UObject properties; this initial array
operation is for arrays of structs, not scalar/object arrays. Existing literal
arrays still replace arrays, and existing `Items`/`Action`/`$Append` behavior is
unchanged. Do not mix append/clear commands with an array `$Patch` envelope.

This reflected array operation is available in `/blueprints`, `/raw`, and the
generic `/assets` property writer. It is not a new JSON merge operator for the
custom recipe/journal/player/spawn definition parsers. Clone directives retain
their existing behavior.

Raw patches keep the `DataTable:RowName` identity, now also supporting arrays of
outer envelopes and direct patch auto-reload for currently loaded tables:

```json
{
  "$Patch": "DT_LootDropTable:Wolf",
  "$Target": {
    "Resources": {
      "$Patch": [{
        "$Match": {
          "SpawnedItemData": "/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Monstrous_Fang.ITEM_Resources_Monstrous_Fang"
        },
        "$Target": { "MinimumDropAmount": 2, "MaximumDropAmount": 3 }
      }]
    }
  }
}
```

A missing raw row is not created. The example runs after Extended Enemy Drops'
existing `$Append` definitions and preserves DropChance, flags and other rows.
If native data or another mod adds a second matching entry, refine `$Match` with
additional fields or inspect the final array and use `$Index`. Raw auto-reload
updates the current table; restart for a complete load-order/streaming replay.
Blueprint patch edits require restart and emit a reminder on auto-reload.

## Blueprint ghost rendering

```json
{
  "$Patch": "BP_Tree_Ash_01_C",
  "$Target": {
    "$VisualEffect": {
      "Type": "Ghost",
      "BodyMaterial": false,
      "MainColor": { "R": 0.2, "G": 0.8, "B": 0.95, "A": 1.0 }
    }
  }
}
```

`$VisualEffect` is RuneSchema metadata and is stripped from ordinary property
assignment. It uses the existing actor-initialization hook after component
changes. It is not applied to class defaults. Matching actors receive the effect
when initialized, including future streamed actors; there is no polling or new
hook. Identical blueprint effects share rooted materials with a game-instance
context. Player/spawn effects use the same extracted renderer with their existing
per-actor tracking and world cleanup.

The optional GhostAshTrees example targets the verified standing
BP_Tree_Ash_01_C and the resource mod's BP_FellableTree_Ash_C. It changes rendering
only. Other blueprint variants or foliage instances require their own matching
rules. BodyMaterial defaults to false; true additionally replaces existing body
slots with MI_Ghost, using Color A/Color B. Overlay colors retain MainColor_Top /
SecondaryColor_Top. FadeControl/custom primitive data is unchanged. Setting
`$VisualEffect` to null clears the configured effect for newly initialized
actors; restart to restore an already affected actor's materials.


See APPEARANCE.md for current layer controls and equipment effects, TOOLS.md for Settings, and CHANGELOG.md for validation.
