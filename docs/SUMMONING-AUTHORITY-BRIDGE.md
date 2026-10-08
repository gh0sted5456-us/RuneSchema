# Self-contained summoning authority bridge

RuneSchema's cooked networking PAKs are transport bridges. A participating mod remains responsible for its own complete gameplay Blueprint implementation and cooked content. Mods should use existing game Blueprint behavior first and call on RuneSchema only where an authority or replication boundary is missing.

For a summoning consumable, the mod owns:

- the shared `BP_SummoningPotionConsumable` class;
- its follower component and follower definitions;
- its monster data table;
- every summoning ItemData asset; and
- one cooked `DA_RuneSchemaRegistry...` asset containing the declarations for those items.

RuneSchema does not own the monster list or follower gameplay. It supplies only the reusable validated network lane. No loose registry file is required after the declaration is embedded in the mod's cooked registry asset.

## Runtime safety

A remote client cannot ask the server to load or execute an arbitrary asset path. RuneSchema observes the real ItemData that completed `OnConsumeSuccess`, sends its stable registry key, and waits for the authoritative inventory to confirm removal of that exact registered item. The server invokes the mod's existing cooked callback only after both events match for the same player.

This preserves the mod's own follower rules, including replacement of the active follower, while the spawned follower continues to use the game's normal replication.

## Cooked registry entry

Place one entry per summoning stone in the JSON stored by the cooked registry asset's `RuneSchemaRegistryJson` string property:

```json
{
  "SchemaVersion": 1,
  "Entries": [
    {
      "Id": "summon_zombie",
      "Kind": "GameplayAuthority",
      "Authority": {
        "Action": "ConsumedItemAuthority",
        "GraphClass": "/Game/Mods/SummoningPotions/BP_SummoningPotionConsumable.BP_SummoningPotionConsumable_C",
        "DataAsset": "/Game/Mods/SummoningPotions/Items/ITEM_SummoningPotion_Zombie.ITEM_SummoningPotion_Zombie",
        "Function": "OnConsumeSuccess"
      }
    }
  ]
}
```

Set the cooked registry asset's `RegistryOwner` to `SummoningPotions`. The stable key for this example becomes `SummoningPotions:summon_zombie`.

All stones may share the same Blueprint class. Each entry points at its own ItemData asset, whose existing `MonsterRow` value selects the follower. The mod builder should generate these entries from the same monster list used to generate the items so the two cannot drift apart.

## Packaging

Cook the shared Blueprint, component, data table, follower definitions, items, and registry asset in the mod's own PAK. Both server and clients need that same PAK and a compatible RuneSchema build. Users do not need to copy or maintain separate per-stone JSON files.
