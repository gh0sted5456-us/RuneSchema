# Equipment, magic and skill authoring

The `/equipment` loader has two layers:

1. Universal reflected item configuration. These are cooked asset properties
   and work in both storefront lanes without executable addresses.
2. Optional native behaviors such as Surge/Dash and Shadowveil. These install
   only when the selected storefront contract and every guarded byte validate.

## Universal item configuration

Vanilla item exports prove that equipment can use `GrantedEffects`,
`AssociatedSkill`, `SkillUsed`, and `SkillPerkRequiredToEquip`. RuneSchema now
exposes all four through one `Items` map:

```jsonc
{
  "Items": {
    "/MyMod/Items/ITEM_MagicCape.ITEM_MagicCape": {
      "AssociatedSkill": "/Game/Gameplay/Character/Player/Skills/SKILL_Magic.SKILL_Magic",
      "SkillUsed": "/Game/Gameplay/Character/Player/Skills/SKILL_Artisan.SKILL_Artisan",
      "SkillPerkRequiredToEquip": "/Game/Gameplay/Character/Player/PerksV2/Magic/PerkV2_Magic_Skillcape.PerkV2_Magic_Skillcape",
      "GrantedEffects": {
        "Mode": "Append",
        "Effects": ["/MyMod/Effects/GE_MagicCape.GE_MagicCape_C"]
      }
    }
  }
}
```

Each item is isolated. A missing property or asset rejects that item rule while
other item rules and loaders continue. `GrantedEffects` also retains its legacy
top-level map for compatibility.

`UtilitySpellData` is not an equipment field. A spell is a modular program of
cast, cost, montage, gameplay-effect, cooldown and optional perk-upgrade
modules. Equipment should grant a cooked gameplay effect that integrates with
those systems; RuneSchema does not pretend a spell path is an item property.

Wearable defense and resistance values live in `DT_WearableEquipment`, not on
the item. Patch those rows through `/raw` or a registry patch so the change is
explicit and conflict-auditable.

## Native behaviors

`SurgeEvadeLegs` and `ShadowveilWearables` remain opt-in compatibility shims.
They cannot silently cross storefront lanes. If a Steam or WinGDK executable
contract does not validate, RuneSchema reports that single capability inactive
and continues reflected equipment, effects, assets, NPC equipment and other
loaders.

