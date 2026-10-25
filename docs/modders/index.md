---
hide:
  - toc
---

# For modders

<p class="rs-hub-lead">Use loader files to connect supported game data, or bring your own cooked assets in a PAK. RuneSchema is a runtime bridge, not a button that cooks Unreal assets for you.</p>

<div class="rs-hub-links">
  <a href="../AUTHORING-GUIDE/">Make your first mod<small>Start with a small loader file.</small></a>
  <a href="first-cooked-item/">Add a cooked item<small>Mount, resolve, register, then test.</small></a>
  <a href="../LOADER-WALKTHROUGHS/">Choose a loader<small>Find the folder and fields for a feature.</small></a>
  <a href="../EXAMPLES/">Browse examples<small>Compare working layouts and definitions.</small></a>
</div>

## Learn with real mod patterns

Start with [FModel export to mod](fmodel-to-mod.md). It explains which parts of
an export identify the object and which parts become a small RuneSchema patch.
The reconstructed export fixtures are labeled; they are not full game dumps.

| Goal | Walkthrough |
| --- | --- |
| Increase chest capacity | [Bigger chests](bigger-chests.md), based on Increased Storage |
| Increase inventory pages and show the slots | [Bigger inventory](bigger-inventory.md) |
| Add a creature drop while keeping its other drops | [Creature drops](creature-drops.md), based on ResourcesAndLoot |
| Change one field on an existing cooked item | [Cooked item patches](cooked-item-patches.md) |
| Create a compatible item variant with its own identity | [Item clones](item-clones.md) |
| Choose replacement, `$Append` or a matched array edit | [Append and targeted patches](append-and-targeted-patches.md) |
| Check saves, compatibility and packaging | [Testing checklist](testing-and-compatibility.md) |
| Generate a distributable Helpy cache from a cooked PAK trio | [Offline cache builder](offline-helpy-cache.md) |

Each tutorial includes a file layout, numbered JSONC, an explanation of the
fields, and a focused test. [Downloadable examples](../examples/Walkthroughs/README.md)
match the code shown in the guides.

## Connect new cooked content

For a cooked feature, check each stage separately: the PAK contains its dependencies, RuneSchema mounts it, the object path resolves, any required native registry accepts it, and the feature works in-game. A successful mount does not establish the later stages.

Start with the guide closest to what you are making:

- [Items, ranged attacks, and magic](../WEAPONS-AND-ITEMS.md)
- [Stations and recipes](../STATIONS-AND-RECIPES.md)
- [Build quests and use Quest Boards](quests-and-questboards.md)
- [Custom armor](../CUSTOM-ARMOR.md)
- [Cooked registry declarations](../COOKED-PAK-REGISTRY-MANIFEST.md)
- [Blueprint and Lua companions](../PSEUDO-LOADERS.md)
- [Guided in-game authoring tools](../ADVANCED-AUTHORING-TOOLS.md)

Before publishing, test a new character and a backed-up existing character. Check first world entry, re-entry, a full restart, and the server/client combinations your mod claims to support.
