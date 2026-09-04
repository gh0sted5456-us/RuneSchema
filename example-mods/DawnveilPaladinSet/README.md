# Dawnveil Paladin Set

A four-piece RuneSchema test mod that combines Paladin plate, a Shadowscale hood,
and a Saradominist cloak. It requires RuneSchema 0.6.1E and does not require a DLL
rebuild.

## What is cloned

| New item | Clone base | Equipped appearance |
| --- | --- | --- |
| Dawnveil Paladin Hood | Paladin's Helm | Male and female Shadowscale Hood mesh-data assets |
| Dawnveil Paladin Plate | Paladin's Platebody | Original Paladin body meshes |
| Dawnveil Paladin Greaves | Paladin Platelegs | Original Paladin leg meshes |
| Dawnveil Oathcloak | Saradominist Cloak | Original Saradominist cloak meshes |

Every item has a new permanent `PersistenceID`, `InternalName`, runtime object path,
display name, flavor text, Power Level 5, durability, and private wearable row. No
vanilla item or shared wearable row is changed.

## Ratings

| Piece | Defense | Melee | Ranged | Magic | Vital Shield |
| --- | ---: | ---: | ---: | ---: | ---: |
| Hood | 18 | 3 | 0 | 3 | 5 |
| Plate | 42 | 6 | -1 | 4 | 10 |
| Greaves | 26 | 4 | 0 | 3 | 5 |
| Oathcloak | 8 | 2 | 2 | 5 | 10 |
| Full set | 94 | 15 | 1 | 15 | 30 |

## Gameplay effect experiment

The oathcloak's `GrantedEffects` array references the native infinite equipment
effect `GE_HeavyArmorSetT1ReduceBlockStaminaCost_C`. The exported effect modifies
`BlockStaminaCostModifierAttribute` by `-0.33`; the inventory-facing `BuffDatas`
entry describes that as 33% lower block stamina cost.

This deliberately grants the existing effect directly while the cloak is equipped;
it does not create or patch a GameplayEffect class. Test the stamina cost before
equipping, with only the cloak equipped, and after unequipping. Also watch for
stacking if another system independently grants the same heavy-armour-set effect.

## Crafting

All four recipes are placed using the demonstrated contract:

```json
{"Table":"DT_CraftingStationsDataTable","Row":"ArmourBench","Category":"Dawnveil Paladin Set"}
```

Recipes use Iron Bars, Hard Leather, Padded Cloth, and Vault Shards. They award the
native Tier 4 forge XP event and create the cloned item aliases declared in
`assets/dawnveil-set.json`.

## Test order

1. Add `DawnveilPaladinSet : 1` to RuneSchema's `mods/mods.txt`.
2. Confirm all four recipes appear at the Armour Bench in one category.
3. Craft and equip each piece, checking names, icons, meshes, and ratings.
4. Confirm the hood uses the Shadowscale skin on male and female characters.
5. Measure block stamina use with and without the oathcloak.
6. Unequip everything, save, reload, and verify all four items retain their identities.

Credits: RuneSchema created by Snorkles; extended features by Jonesing4Space.
