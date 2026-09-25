# Helpy collection-style experiment

Branch: `super-experimental-helpy-collection-ui`

This branch changes **Helpy only**. RuneSchema loaders, schemas, gameplay
mutation logic, networking, and the core runtime are not redesigned here.

## Reference direction

The visual and interaction direction is based on the supplied Collection Log
2.0.2 mod:

- near-black warm surfaces instead of blue-grey panels;
- parchment/cream primary text;
- restrained gold for focus, selection, masterwork state, and rail accents;
- flat cards and rows instead of stacked faux borders;
- persistent section rail;
- lightweight hover states;
- expensive artwork resolved incrementally.

The reference mod is not copied or required at runtime.

## Performance changes

Helpy remains on the existing game-viewport Canvas lifecycle. This experiment
does not replace the proven input/world ownership layer.

The presenter now:

- retains the last generated draw/hit frame while the model and pointer are
  unchanged;
- uses the actually presented frame for click/right-click hit testing;
- invalidates the retained frame after model input, catalog updates, world/menu
  generation changes, and section changes;
- resolves at most 6 new visible icons on the first frame and 2 on later frames;
- uses one flat fill for normal compact item placards and one optional 3 px
  state accent;
- permits viewport scaling up to 2.5x so Helpy does not remain 720p-sized on
  high-resolution displays.

Canvas still submits visible primitives each PostRender. The retained frame
removes command-tree regeneration, not Unreal's final paint pass.

## Build

For Helpy only, double-click:

`Build Helpy.bat`

That calls the existing root build pipeline with `-PluginOnly`. It compiles
`RuneSchemaHelpyPlugin`, runs the Helpy release contract, and writes:

`dist/RuneSchema.Helpy-0.7.5.28.zip`

For the complete RuneSchema packages, the existing:

`Build RuneSchema.bat`

still builds both `RuneSchema` and `RuneSchemaHelpyPlugin`.

## Test focus

Before promoting this branch, test:

- 720p, 1080p, 1440p, ultrawide, and 4K;
- F2 open/close, Escape, alt-tab, and world travel;
- mouse hit alignment at each resolution;
- rapid search, paging, favorites, item selection, NPC/resource tabs;
- first open with an empty icon cache;
- item details and Item Lab overlays;
- clean shutdown and input restoration.

The goal is a faster-feeling Helpy with a substantially cleaner visual language
without changing command semantics.
