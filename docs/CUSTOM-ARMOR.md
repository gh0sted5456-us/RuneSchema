# Create custom cooked armor

## Step-by-step workflow

1. Prepare a compatible wearable ItemData and the male/female mesh-data records for each intended slot.
2. Cook the meshes, materials, textures and required body/hair visibility rules together.
3. Mount/register the item and wearable references, then add crafting/discovery through their owning loaders.
4. Restart and check both body variants, equip/unequip, death/respawn, remote presentation and saved inventory.

JSONC snippets are authoring mock-ups. Replace example paths, IDs and values with
inspected content; a valid snippet cannot supply missing cooked dependencies.

**Tie-ins:** [Assets](reference/loaders/assets.md), [equipment](reference/loaders/equipment.md), [raw rows](reference/loaders/raw.md) and [cooked registries](COOKED-PAK-REGISTRY-MANIFEST.md).


RuneSchema can help a cooked armor item join Dragonwilds' native
wearable presentation path. The armor mod still owns its item, meshes,
materials, textures, and appearance rules. RuneSchema does not paint an item
or invent a material for it.

## Author the complete armor set

For each armor item, provide a stable cooked item asset and the game's required
item identity. Keep its existing PersistenceID and InternalName when updating a
released item, so existing characters do not lose it.

Bind the item to its intended male and female wearable mesh-data assets. Do
this for every slot your mod adds: head, body, legs, and any other supported
wearable slot. Each mesh-data asset must point to the correct cooked mesh and
any required material override, opacity mask, and section-hide rules. Body
and leg pieces also need the game's appropriate authored undergarment-hide
rules; helmet hair rules do not substitute for those. Cook the materials **and their
textures** into the PAK; a path to an uncooked material is not enough. Check
that each asset resolves by its full object path after mounting.

For a dyed set, bind the dyed material directly through the custom mesh and
mesh-data assets. A custom item that still points to a vanilla wearable mesh
data asset can display vanilla colors even though its item registry entry is
valid. The new bridge does not replace a mod's dye-selection logic.

## Let RuneSchema connect the native wearable path

RuneSchema registers eligible cooked wearable items in the native item-name
enumeration map after their normal persistence and network identities resolve.
When an equipped custom mesh-data asset is missing from the player's native
material-hide cache, RuneSchema asks the game's own cache builder to populate
it. This may happen again after an equipment change invalidates that cache.
Dragonwilds then evaluates the item's authored hide rules for its slot,
including helmet hair or body/leg undergarment rules. RuneSchema does not
fabricate or copy the cache's internal values.

You may declare cooked wearable mesh data in your PAK's `RSREG_` or
`DA_RuneSchemaRegistry` asset. This resolves and class-checks the declared
dependencies early; it does not substitute for the item bindings above:

```jsonc
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": [
      "/MyArmor/Items/ITEM_BlackHead.ITEM_BlackHead"
    ],
    "WearableMeshData": [
      "/MyArmor/MeshData/WEMD_BlackHead_Male.WEMD_BlackHead_Male",
      "/MyArmor/MeshData/WEMD_BlackHead_Female.WEMD_BlackHead_Female"
    ]
  }
}
```

The `WearableMeshData` list is optional. It is a bounded loading and validation
aid, **not** a separate save registry or a direct instruction to hide a
material. Check `[PAK-PRESENTATION]` and `[WEARABLE-BRIDGE]` messages if the
armor loads but its appearance is wrong. Do not add thousands of unrelated
assets merely to make them load.

## Compatibility boundary

Native cache refresh is available only for executable profiles whose identity
and function signatures match the supported Steam/Epic or Game Pass build.
On an unmatched game update, this appearance helper stays inactive while
ordinary item registration continues. A registered item does not prove that
its cooked mesh, material, or hide predicates are complete. If a slot shows
an exposed undergarment or fails to restore it on unequip, check the authored
mesh-data rules and full dependency paths before changing RuneSchema settings.

For multiplayer, each player who must see the armor needs its cooked assets;
the server remains authoritative for gameplay equipment state.

For the broader PAK workflow, see [Create PAK content](PAK-CONTENT-GUIDE.md)
and the [cooked registry manifest](COOKED-PAK-REGISTRY-MANIFEST.md).
