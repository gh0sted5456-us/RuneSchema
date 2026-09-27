# Unreal + RuneSchema

Some Dragonwilds mods need both **cooked Unreal content** and **RuneSchema
definitions**. Unreal supplies the real meshes, materials, actors, collision,
components, and cooked data assets. RuneSchema registers those objects with the
game, connects recipes and registries, exposes build-menu entries, and handles
the surrounding authoring logic.

<div class="grid cards" markdown>

-   :material-hammer-screwdriver:{ .lg .middle } **Stations, quests & recipes**

    ---

    A complete Coinage case study covering independent crafting and processing
    stations, building-piece registration, recipes, runtime item identities,
    quest chains, dialogue, packaging, and verification.

    [:octicons-arrow-right-24: Open guide](stations-quests-recipes.md)

</div>

## When to use this section

Use these guides when the mod crosses the boundary between authored Unreal
objects and RuneSchema JSON/JSONC definitions. Typical examples include:

- a custom station actor with its own private crafting or processing row;
- a cooked `BuildingPieceData` registered through RuneSchema;
- recipes that target a custom station row;
- cooked visual assets referenced by a RuneSchema-created item;
- quest or dialogue flows built around custom actors or items;
- packaging rules for cooked mod content that RuneSchema consumes at runtime.

For loader-only work that does not require cooked Unreal content, use the
[Loader Reference](../LOADER-WALKTHROUGHS.md).
