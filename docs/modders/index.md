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

## Follow the content chain

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
