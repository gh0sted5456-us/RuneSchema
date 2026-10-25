# Walkthrough: make one cooked item discoverable

This walkthrough starts **after** you have cooked an `ItemData` asset in a
Windows PAK. It shows the smallest RuneSchema connection. For the Unreal
project, cooking, and asset dependencies, follow the full
[PAK authoring guide](../PAK-CONTENT-GUIDE.md).

## 1. Record the permanent identity

Give the item a unique internal name and persistence ID in its cooked data.
Record its full object path, including the object name after the dot:

```text
/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear
```

This is an example path. Replace it with the path **inside your PAK**. Keep
the path, internal name, and persistence ID stable after players have saved
the item. Do not recycle the ID for a different asset.

## 2. Put the PAK in its owning mod

```text
RuneSchema/mods/GoldenArsenal/
  assets/10-golden-spear.jsonc
  paks/GoldenArsenal/
    GoldenArsenal.pak
    GoldenArsenal.utoc
    GoldenArsenal.ucas
```

Keep only files your cook actually produced; not every package has all three
extensions. Do not copy the same PAK into the game's ordinary PAK folder as
well. Enable `GoldenArsenal : 1` in `RuneSchema/mods/runeschema.txt`.

## 3. Add one direct item reference

In `assets/10-golden-spear.jsonc`:

```json
{
  "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear": {}
}
```

The empty object asks RuneSchema to resolve this cooked item without changing
its properties. It cannot create an item missing from the PAK or repair an
incomplete cooked class. For very large catalogues, use normal cooked
relationships and the supported [registry declaration](../COOKED-PAK-REGISTRY-MANIFEST.md)
instead of repeating this file thousands of times.

If the PAK has no declaration, a [loose `registry` sidecar](../reference/loaders/registry.md#register-a-cooked-item-without-cooking-a-registry-declaration)
can name the existing item without recooking it. For a property change rather
than enrollment alone, follow [patching a cooked item](cooked-item-patches.md).
Use one appropriate enrollment route rather than duplicating declarations.

## 4. Make it obtainable

Item registration alone does not add a crafting recipe. Add a `recipes` file
only when the item should be made at a station, and use the full item path in
`ItemsCreated`. The `AddTo` target must name a real compatible station table,
row, and category or recipe array. Work through the
[crafting and processing examples](../STATIONS-AND-RECIPES.md).

If your PAK already has a complete native recipe and unlock path, check that
path in-game before adding a duplicate RuneSchema recipe.

## 5. Verify one boundary at a time

1. Start with only this mod and its required dependencies enabled.
2. In `UE4SS.log`, find the mod name, PAK mount, item path resolution, and
   item registration summary. Fix the first missing step.
3. Confirm the item can be obtained, equipped or used, saved, and reloaded.
4. If it is a weapon, separately verify its held actor, attack family,
   animation, damage, and ammo or spell path. [Weapon guide](../WEAPONS-AND-ITEMS.md).
5. If it is wearable armor, verify each authored material and body mesh.
   [Armor guide](../CUSTOM-ARMOR.md).

Test a backed-up existing save before release. A mounted PAK is not proof that
every item in it joined a live registry.
