# Appearance in mod JSON

Ghost rendering and overlay glow share the existing material renderer. These are cosmetic material changes, not spell enchantments, character-creation skin colors or arbitrary particle effects.

```json
{ "Type": "Ghost", "Overlay": true, "BodyMaterial": true,
  "MainColor": { "R": 0.2, "G": 0.8, "B": 1.0, "A": 1.0 },
  "SecondaryColor": { "R": 0.05, "G": 0.2, "B": 0.5, "A": 1.0 } }
```

Overlay defaults to true and enables the ghost shader's glow overlay. BodyMaterial defaults to false; true replaces physical mesh material slots with the baked ghost body material. Either may be enabled alone or both together. Both false is rejected; remove/null the rule instead. RGBA colors retain their existing ranges. Widgets/nameplates are excluded.

For /players and /spawns use VisualEffect. A player can set Target to PlayerMesh (body/head only) or EntirePerson (default, including armor and held items). For /blueprints use $VisualEffect in the matching class definition or $Patch/$Target envelope. GhostAshTrees demonstrates standing and fellable ash actors. Streaming foliage that is not an actor requires a different pathway.

For equipment use $VisualEffect inside its /assets definition:

```json
{
  "$Patch": "/Game/Gameplay/Character/Player/Equipment/Cape/ITEM_Cape_Artisan.ITEM_Cape_Artisan",
  "$Target": { "$VisualEffect": { "Type": "Ghost", "Overlay": true, "BodyMaterial": true } }
}
```

The same field works beside $Clone. It is RuneSchema metadata, never written as an unknown Unreal property. Equipment rules automatically target that item's worn head/body/legs/cape mesh or physical meshes on its held left/right actor. Omit Target for equipment. Inventory icons and hidden/stowed models are not changed.

A character can remain normal while only Ghostly Warden armor glows. A PlayerMesh ghost can coexist with independently tinted armor and weapons. A specific item rule overrides a whole-person rule for that item's mesh. Removing the specific rule falls back to the player rule, if present. Shadowveil stealth takes priority; appearance resumes through existing appearance events. Mesh changes capture new materials; outgoing equipment restores only materials still owned by RuneSchema.

Put item appearance patches in ZZ_ folders after item definitions. $VisualEffect: null removes that equipment rule; it does not erase the underlying item or a broader player rule. Restart when adding rules or changing load order; file deletion is not a live undo system. Keep the World / player runtime loader enabled for equipment refresh. /equipment is not required for cosmetic effects.

Identical styles share material instances. Player material ownership is bounded and weak actor tracking avoids address-reuse mistakes. Spawn and blueprint style caches have a 256-style limit. Dedicated servers skip cosmetic material loading/hooks; rendering clients need the appearance JSON. Cloned item identities/recipes must be installed consistently on server and clients. This does not establish tested dedicated-server or cross-version compatibility for all native equipment hooks.

Optional examples: GhostlyWarden creates four distinct craftable equipment clones from existing armor/cape data, retaining base stats; GhostStaff adds an overlay to the observed Pharaoh's Sceptre; GhostAshTrees modifies ash appearance. Examples are not enabled automatically.
