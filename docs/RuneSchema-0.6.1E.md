# RuneSchema 0.6.1E

**UE4SS Stack Limit Fix Dependent**

RuneSchema 0.6.1E is released by **Jonesing4Space**. It extends RuneSchema 0.6.0 by **Snorkles**, which is based on **PalSchema** by Okaetsu.

## What 0.6.1E provides

- JSON and JSONC mod loaders for assets, raw tables, Blueprints, recipes, journal entries, buildings, spawns, courses, strings, and player rules.
- `$Patch` field updates and clone support with deterministic `AA_` and `ZZ_` mod ordering.
- Asset-authored appearance effects for equipment, characters, spawns, Blueprints, weapons, armor, and resource nodes.
- Equipment runtime behavior for validated Surge evade and Shadowveil stealth rules, with separate client and dedicated-server contracts.
- Native AI spawns, resource nodes, grounding, loot rows, display names, combat scaling, drop scaling, and visual effects.
- Manual inspection, export, trace, and preset tools. Diagnostics are opt-in and may affect performance.
- Consolidated normal logging. Spawn output reports counts for AI, bosses, resource nodes, other actors, and removals. Detailed success narration requires **Advanced verbose logging**.
- Preferred cooked-content layout: `mods/<ModName>/paks/<PackName>/`. The older named-folder layout remains supported.

## Installation

Install the matching UE4SS StackFix host supplied with this release, then place RuneSchema under `ue4ss/Mods/RuneSchema/`. Replace DLLs only while the game and dedicated server are stopped.

`version.dll` in the supplied UE4SS package is the dedicated-server entry point. Single-player clients ignore it.

## Authoring boundaries

Use `/assets` for item properties, cloned item data, perk icons, recipes, and appearance effects. Use `/equipment` only for validated runtime behavior that data edits cannot express. Use `/spawns` for world actors and AI spawn points.

## Status

0.6.1E is an experimental release candidate. A matching game executable and matching UE4SS host are required for native equipment behavior. Repeated client and server launches, world entry, GUI use, and normal exit remain the practical validation for a mod set.

See the focused guides for [appearance](APPEARANCE.md), [patching](PATCHING.md), [equipment](EQUIPMENT.md), [tools](TOOLS.md), [packs](PAKS.md), and [building](BUILDING.md).
