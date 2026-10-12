# RuneSchema for modders

RuneSchema is a way to connect authored content to Dragonwilds, not a cook
button for Unreal assets. Use JSON/JSONC to adjust supported game data; use a
PAK for new meshes, items, Blueprints, sounds, or other cooked content. Keep
the gameplay asset and its dependencies in your own mod where possible.

## Choose your starting path

| You want to… | Start with… | Then check… |
| --- | --- | --- |
| Change existing game values | [First mod](../AUTHORING-GUIDE.md) | [Loader reference](../LOADER-WALKTHROUGHS.md) |
| Ship one new cooked item | [First cooked item walkthrough](first-cooked-item.md) | [PAK guide](../PAK-CONTENT-GUIDE.md) |
| Make a weapon attack or spell | [Weapons and items](../WEAPONS-AND-ITEMS.md) | [Cooked registry declarations](../COOKED-PAK-REGISTRY-MANIFEST.md) |
| Build a station and place recipes | [Stations and recipes](../STATIONS-AND-RECIPES.md) | [Buildings loader](../reference/loaders/buildings.md) |
| Add custom armor | [Armor walkthrough](../CUSTOM-ARMOR.md) | [Equipment loader](../reference/loaders/equipment.md) |
| Build NPCs, quests, and dialogue | [Loader folder guide](../LOADER-WALKTHROUGHS.md) | [NPC](../reference/loaders/npc.md), [quest](../reference/loaders/quests.md), and [dialogue](../reference/loaders/dialogue.md) |
| Add client-only logic | [Blueprint and Lua companions](../PSEUDO-LOADERS.md) | [Lua reference](../UE4SS-LUA-COMPANIONS.md) |

## The content chain

For a new cooked feature, verify each link separately:

1. **Cooked:** the PAK contains the asset and every required dependency.
2. **Mounted:** RuneSchema loaded the enabled PAK from the intended mod folder.
3. **Resolved:** the exact object path names a compatible cooked asset.
4. **Registered:** the item, recipe, station, attack, or spell appears in its
   applicable live game list when that feature needs registration.
5. **Used:** the feature works in-game, including save/reload and multiplayer
   where your mod supports them.

An empty `assets` reference can load one exact cooked item path. A large
catalogue should keep normal relationships in cooked content and use a
mod-owned [registry declaration](../COOKED-PAK-REGISTRY-MANIFEST.md) only for
supported missing enrollment paths. Do not create thousands of empty JSON
files to stand in for a complete PAK.

## Authoring desk

- [Guided in-game authoring tools](../ADVANCED-AUTHORING-TOOLS.md)
- [Folder and loader guide](../LOADER-WALKTHROUGHS.md)
- [JSON field schemas](../SCHEMAS.md)
- [Example library](../EXAMPLES.md)
- [Mod-host file types and layout](../MOD-HOSTS-AND-GETTING-STARTED.md)
- [Troubleshooting from the first failed stage](../TROUBLESHOOTING.md)

Before publishing, test with a fresh character and an existing backed-up
character. Test first world entry, return to menu, re-entry, a full restart,
and any server/client combination your mod claims to support.
