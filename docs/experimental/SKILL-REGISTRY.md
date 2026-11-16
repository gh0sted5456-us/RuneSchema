---
channel: experimental
promote_to: SKILL-REGISTRY.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Native skill registration investigation

Status: read-only discovery completed on an isolated Steam 1.0.1.2 fixture on
October 29, 2026. No skill identities, enum members, saves or production
installations were changed. This establishes reflected structure, not proof of
every compiled consumer. New native Skills/SkillPerks enrollment remains blocked.

## What the running game exposes

| Contract | Observed behavior or structure |
| --- | --- |
| SkillSubsystem | GameInstanceSubsystem; inherits the five Dominion data-registry containers. Exposes GetXPNeededForLevel and GetLevelFromXp. |
| SkillPerkSubsystem | Same inherited registry containers; no additional reflected fields observed on the native class. |
| SkillData | PersistenceID/InternalName plus SkillType, MaxLevel, MinorPerkConfigs, DamageXPEvent, modifier attributes, artwork and TrimmedCapeRecipe. |
| SkillPerkData | PerkModules, PerkUnlockInfo, FeatureTag and the normal identity/presentation fields. |
| SkillComponent | Skills array, SkillsToXPModifierAttributes, XPBatches, DefaultAttackSkill, native level/XP queries, AddXpFromEvent and OnRep_Skills. |
| Skill record | CurrentXP plus a SkillData object reference. |
| ESkill | Invalid=0; named values 1 through 16; ESkill_MAX=17. The list includes currently separate/native plugin skills such as Agility and Fishing. |

The probe found 15 loaded SkillData objects at the menu. That number is a
snapshot, not an enumeration contract or a list of every future skill.
Game-owned plugins already provide skills from mounts such as `/Agility/` and
`/Fishing/`; their full native integration is not evidence that extending the
enum at runtime recreates those consumers.

The UI uses a Blueprint-derived SkillComponent even at the character menu.
RuneSchema's existing character-preview/workbench code reads native SkillData
and XP thresholds. It does not construct new progression records. Existing save
edits also require an already-present saved skill ID and verified thresholds.

## Why adding to the registry is insufficient

Appending to PersistenceIDToDataMap, InternalNameToDataMap, AssetNameToDataMap,
NetIdToData and DataToNetIdMap would expose lookups. It would not establish:

- allocation/initialization of a new skill record on fresh and existing characters;
- how compiled ESkill switches, fixed arrays, MAX checks and attributes handle a
  new numeric value;
- the skill menu's ordering, visibility and perk-tree ownership;
- native XP event routing, batching, replication and level-up callbacks;
- perk startup caches, unlock requirements and feature-tag handling;
- missing-mod save behavior and identical client/server skill IDs.

Borrowing another native SkillType can silently redirect progression to that
skill. Adding a reflected enum name does not rewrite compiled C++ consumers.
Keep the explicit ESkill guard; do not advertise staged declarations as support.

## Smallest implementation worth testing

1. Cook a dedicated SkillData, perk fixture, XP event row, menu assets and
   gameplay consumers. Use stable owned identities; keep behavior in the PAK.
2. Capture native initialization for a game-owned plugin skill and reproduce its
   actual record/menu/attribute relationships. Verify the compiled enum boundary
   on both executables before assigning any additional ESkill value.
3. Only then add a dedicated skill adapter at GameInstance startup. Stage all
   identities, reject duplicate SkillType/aliases, retain native IDs, and publish
   forward/reverse network mappings with the existing authority transaction.
4. Initialize progression through a verified native constructor/lifecycle path.
   Do not push an invented JSON or raw byte record into a replicated array.
5. Test a fresh character, an old copied character, XP events, level-up, perk
   unlock, save/reload, missing provider, late join and reconnection. Include
   Steam, Game Pass and a dedicated server.

If compiled consumers impose a closed enum, the alternative is a mod-owned
progression component/UI with cooked rules and stable mod persistence. That is
a different feature from a native skill registry and should be named as such.

For now, use supported edits to existing native assets and tables, and existing
[player/world hooks](COOKED-NATIVE-DATA-FAMILIES.md) for cooked behavior. These
routes do not add a new skill or bypass the staged Skills/SkillPerks guard.

The capture used the documented UE4SS
[UStruct iteration](https://docs.ue4ss.com/dev/lua-api/classes/ustruct.html) and
[UEnum inspection](https://docs.ue4ss.com/dev/lua-api/classes/uenum.html) APIs.
