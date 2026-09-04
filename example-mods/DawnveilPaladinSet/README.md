# Dawnveil Paladin Set

A seven-piece RuneSchema test mod that combines Paladin plate, a Shadowscale hood,
and a Saradominist cloak. It requires RuneSchema 0.6.1E and does not require a DLL
rebuild.

## What is cloned

| New item | Clone base | Equipped appearance |
| --- | --- | --- |
| Dawnveil Paladin Hood | Paladin's Helm | Male and female Shadowscale Hood mesh-data assets |
| Dawnveil Paladin Plate | Paladin's Platebody | Original Paladin body meshes |
| Dawnveil Paladin Greaves | Paladin Platelegs | Original Paladin leg meshes |
| Dawnveil Oathcloak | Saradominist Cloak | Original Saradominist cloak meshes |
| Dawnveil Oathsword | Steel Sword | Original steel sword actor, icon, and mesh |
| Dawnveil Aegis | Steel Shield | Original steel shield actor, icon, and mesh |
| Dawnveil Luminary Staff | Blightwood Battlestaff | Original battlestaff actor, icon, and mesh |

Every item has a new permanent `PersistenceID`, `InternalName`, runtime object path,
display name, flavor text, Power Level 10, and durability. The four wearables use
private stat rows; the three weapons use their native direct combat fields. No
vanilla item or shared row is changed.

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
entry describes that as 33% lower block stamina cost and uses the Amulet of Defense
icon rather than an empty texture reference.

The Luminary Staff separately grants the native infinite
`GE_Trinket_IncreaseMagicDamage_AmuletOfMagic_C` effect. Its exported value is
`0.125` on `DamageModifierWeaponTypeStaff`, so its visible buff reports 12.5% extra
staff magic damage and uses the Amulet of Magic icon.

This deliberately grants the existing effect directly while the cloak is equipped;
it does not create or patch a GameplayEffect class. Test the stamina cost before
equipping, with only the cloak equipped, and after unequipping. Also watch for
stacking if another system independently grants the same heavy-armour-set effect.

## Crafting

The armor, sword, and shield use the Armour Bench; the staff uses the Mystic Forge.
Every recipe follows the demonstrated placement contract, for example:

```json
{"Table":"DT_CraftingStationsDataTable","Row":"ArmourBench","Category":"Dawnveil Paladin Set"}
```

Recipes use Iron Bars, Hard Leather, Padded Cloth, Blightwood, Wild Anima, and Vault
Shards. They award the native Tier 4 forge XP event and create the cloned item
aliases declared in `assets/dawnveil-set.json`.

## Important raw-table ordering note

The observed 0.6.1E log applied `DT_WearableEquipment` after reading the first
enabled mod's raw data but before later enabled mods finished registering their raw
rows. That launch reported exactly the ten `RuneschemaCapes` rows and no Dawnveil
rows. Until that loader timing is corrected, place this mod first in `mods.txt`:

```text
DawnveilPaladinSet : 1
RuneschemaCapes : 1
FourPlayerProfiles : 1
NamedSpawnShowcase : 1
```

This makes the four Dawnveil rows available when the wearable table serializes.
It is a load-timing workaround, not a JSON schema change. If testing the cape demo
stats simultaneously, disable that pack or expect its later raw rows to miss the
same early serialization window.

## Test order

1. Put `DawnveilPaladinSet : 1` first in RuneSchema's `mods/mods.txt`.
2. Confirm six recipes appear at the Armour Bench and the staff at the Mystic Forge.
3. Craft and equip each piece, checking names, icons, meshes, ratings, and weapon fields.
4. Confirm the hood uses the Shadowscale skin on male and female characters.
5. Confirm the oathcloak tooltip has the defense icon and measure block stamina use.
6. Confirm the staff tooltip has the magic icon and compare staff spell damage.
7. Unequip everything, save, reload, and verify all seven items retain their identities.

Credits: RuneSchema created by Snorkles; extended features by Jonesing4Space.
