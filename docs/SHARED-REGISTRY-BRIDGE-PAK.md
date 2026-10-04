# Shared registry bridge PAK

RuneSchema's shared registry bridge PAK provides one stable cooked-content base:

```text
/RuneSchema/Registry/PDA_RuneSchemaRegistryBridgeBase
```

It is a template for registry declarations, not a global table that every mod
edits. Each mod creates and cooks its own data-asset instance. This lets many
content PAKs register together without replacing one another.

## What the shared bridge solves

Mounting a PAK makes its files available, but Dragonwilds does not automatically
place every item, recipe, spell, or attack into the live gameplay registry that
uses it. The bridge gives a content author one small asset that explicitly names
the cooked content their PAK needs RuneSchema to verify and register.

RuneSchema still treats the game's live registries as authoritative. If the game
has already registered an entry, RuneSchema accepts it without adding a second
copy. If an identity conflicts with a different object, RuneSchema rejects that
lane instead of guessing.

## One bridge asset per mod

Do not edit or replace `PDA_RuneSchemaRegistryBridgeBase`. Do not have several
mods cook the same shared bridge asset path.

For each content mod:

1. Add RuneSchema's **uncooked authoring plugin** to the Unreal project. The
   cooked runtime PAK cannot be edited or used as an Unreal Editor template.
2. In Unreal Editor, create a **Data Asset** using
   `PDA_RuneSchemaRegistryBridgeBase` as its class.
3. Give the asset a unique name beginning with
   `DA_RuneSchemaRegistry`, such as
   `DA_RuneSchemaRegistry_Flintlock`.
4. Store it below the mod's permanent mount point, for example:

   ```text
   /Flintlock/Registry/DA_RuneSchemaRegistry_Flintlock
   ```

5. Set `RegistryOwner` to a permanent identifier containing only letters,
   numbers, `_`, `-`, or `.`, such as `Flintlock`.
6. Paste the compact declaration into `RuneSchemaRegistryJson`.
7. Cook the bridge instance in the same PAK as the assets it names.

The shared base belongs only in RuneSchema's PAK. The mod-owned data-asset
instance belongs in the mod's PAK.

## Authoring files and runtime files

RuneSchema distributes the bridge in two forms:

- **Authoring:** the uncooked `RuneSchema` content plugin containing the editable
  `PDA_RuneSchemaRegistryBridgeBase` asset. Copy this plugin into the Unreal
  project used to build the mod.
- **Runtime:** `RegistryBridge_P.pak`, `RegistryBridge_P.utoc`, and
  `RegistryBridge_P.ucas`. Install all three together. Players and dedicated
  hosts need these files; they do not need the uncooked authoring asset.

Do not put the uncooked asset into the installed game, and do not import the
cooked runtime files into an Unreal project. A mod should inherit from the
authoring copy, then cook only its uniquely named `DA_RuneSchemaRegistry_*`
instance into the mod's own PAK.

## Example bridge declaration

This example asks RuneSchema to verify one item, its recipe, two combat spells,
and an ordered melee pair:

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": [
      "/Flintlock/Items/ITEM_Flintlock.ITEM_Flintlock"
    ],
    "Recipes": [
      "/Flintlock/Recipes/RECIPE_Flintlock.RECIPE_Flintlock"
    ],
    "Quests": [],
    "CombatSpells": [
      "/Flintlock/Spells/SPELL_FlintlockPrimary.SPELL_FlintlockPrimary",
      "/Flintlock/Spells/SPELL_FlintlockHeavy.SPELL_FlintlockHeavy"
    ],
    "UtilitySpells": [],
    "EquipmentEffects": [],
    "MeleeAttackClasses": [
      "/Flintlock/Attacks/BP_FlintlockBashQuick.BP_FlintlockBashQuick_C",
      "/Flintlock/Attacks/BP_FlintlockBashFull.BP_FlintlockBashFull_C"
    ],
    "RangedAttackClasses": []
  }
}
```

All paths are object paths, not Windows file paths. Class paths end in `_C`.
Ordinary item, recipe, spell, and effect assets do not.

## Ranged weapons

Dragonwilds owns ranged attacks through the registered item's
`HeldEquipmentData.RangedAttackCollection`. Therefore a ranged weapon normally
declares its `ItemData` in `Items` and keeps the cooked links below intact:

```text
ItemData
  -> HeldEquipmentData
     -> RangedAttackCollection
        -> QuickAttackData
        -> FullAttackData
```

Both `QuickAttackData` and `FullAttackData` must resolve to valid player attack
classes. RuneSchema rejects a quick-only or full-only collection. It does not
invent a missing half, and it does not register lower-level projectile or shot
implementation classes as player attacks.

If a weapon needs a second pair for attacks three and four, that pair must also
be reachable through a legitimate, mod-owned registered item/equipment
collection. A hidden collection that nothing references cannot be discovered
reliably.

`RangedAttackClasses` remains reserved for compatibility and should be left
empty. It is not a substitute for the item-owned ranged collection.

## Magic weapons

Put persistent spell data in `CombatSpells` or `UtilitySpells`. Keep the cooked
item relationship to `HeldEquipmentData.MagicAttackCollection`, which selects
those registered spells while the weapon is equipped.

Do not place spells in `MeleeAttackClasses` or `RangedAttackClasses`.

## Several mods using the bridge

Every mod has its own asset, owner, mount point, and declaration. RuneSchema
processes enabled mods in `runeschema.txt` order and reconciles each declaration
against the same live game registries.

This is safe when:

- each mod uses permanent, unique object paths and persistence identities;
- each mod ships its own bridge instance rather than the shared base;
- complete ordered collections are declared in their required order;
- server and clients install the same gameplay PAKs in the same order;
- no two mods claim the same identity for different objects.

If the game begins registering the same cooked content by itself, RuneSchema
recognizes it as already present. It does not append a duplicate.

## Enabling and disabling

The content PAK and its RuneSchema mod folder should be controlled by the same
entry in `runeschema.txt`. A mod marked `0` must not contribute its bridge asset
or registry declarations. Do not copy the bridge instance into a second enabled
folder as a fallback.

Removing a mod removes its cooked assets. On the next clean game launch,
RuneSchema validates the settled live registries and the persistence pruner may
remove only identities that no applicable live registry can resolve.

## Expected log messages

A healthy bridge produces concise messages similar to:

```text
Registry: discovered cooked registry asset '/Flintlock/Registry/DA_RuneSchemaRegistry_Flintlock' owned by 'Flintlock'.
[PAK-REGISTRY][DECLARED] owner='Flintlock' ...
[PAK-REGISTRY][ITEM][READY] ...
[COMBAT-REGISTRY][RANGED-EQUIPMENT][READY] ...
```

Treat `REJECTED`, `CONFLICT`, `INCOMPLETE`, or `MISSING` as an authoring problem.
RuneSchema will leave that unsafe lane unchanged.

## Packaging checklist

- The shared RuneSchema PAK contains
  `PDA_RuneSchemaRegistryBridgeBase` exactly once.
- The mod PAK contains its uniquely named `DA_RuneSchemaRegistry_*` instance.
- `RegistryOwner` is stable and is not `RuneSchema`, `FModel`, or `Game`.
- `RuneSchemaRegistryJson` is valid JSON and smaller than 256 KiB.
- Every declared object is cooked into an enabled PAK.
- The ranged item resolves a complete quick/full collection.
- Server and clients use matching PAKs and load order.
- World exit and re-entry are tested; only a pre-existing component in the first
  gameplay world may use the bounded live fallback. Later worlds are read-only.

For the full lane limits and validation rules, see the
[advanced cooked PAK registry guide](COOKED-PAK-REGISTRY-MANIFEST.md).
Advanced users can independently disable only RuneSchema's optional weapon
helpers through the [combat fallback settings](COMBAT-FALLBACK-SETTINGS.md).
