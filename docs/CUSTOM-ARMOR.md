# Create custom cooked armor

RuneSchema 0.7.7.8 can help a cooked armor item join Dragonwilds' native
wearable presentation path. The armor mod still owns its item, meshes,
materials, textures, and appearance rules. RuneSchema does not paint an item
or invent a material for it.

## Author the complete armor set

For each armor item, provide a stable cooked item asset and the game's required
item identity. Keep its existing PersistenceID and InternalName when updating a
released item, so existing characters do not lose it.

Bind the item to its intended male and female wearable mesh-data assets. Each
mesh-data asset must point to the correct cooked mesh and any required material
override, opacity mask, and section-hide rules. Cook the materials **and their
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
it. Dragonwilds then evaluates its authored hide rules, including helmet hair
rules. RuneSchema does not fabricate or copy the cache's internal values.

You may declare cooked wearable mesh data in your PAK's `RSREG_` or
`DA_RuneSchemaRegistry` asset. This resolves and class-checks the declared
dependencies early; it does not substitute for the item bindings above:

```json
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

## Compatibility and testing

The native cache-refresh hook in 0.7.7.8 has been verified in game on the
inspected Steam executable profile (PE timestamp `0xd702bba2`, image size
`0xddf1000`). The 0.7.7.8e experimental build also carries a strictly
signature-matched profile for the active Game Pass/WinGDK 100.8.0.0 image
(PE timestamp `0xb0da0883`, image size `0x0db4e000`). Its native function
entry points were checked against the running image and x64 unwind table. A
Game Pass test loaded RuneSchema, entered a world, and showed the custom armor
material and hidden hair. Its log did not show wearable-bridge activation, so
that visual result cannot yet be attributed to the bridge. A profile mismatch
leaves cache refresh inactive; ordinary item registration remains available.

The confirmed in-game case is one TEST ARMOR helmet: its authored black
material displayed and its hair was hidden, including after two former
presentation modules were removed. That does **not** yet establish helmet
removal, every armor slot, character preview, multiplayer, dedicated server,
or every dye variant. Test equip and unequip, male and female appearances,
world re-entry, client/server play, and mod removal on a backed-up save before
shipping your own armor.

For the broader PAK workflow, see [Create PAK content](PAK-CONTENT-GUIDE.md)
and the [cooked registry manifest](COOKED-PAK-REGISTRY-MANIFEST.md).
