# Helpy rendering and cache notes

This page records Helpy implementation decisions and performance targets.

## Direction

Keep Helpy on the real game viewport.

Do **not** clone or depend on a Dragonwilds-owned menu. Game-owned widgets bring
private classes, cooked assets, focus rules, and version-specific paths into the
plugin.

Current direction:

- keep `UGameViewportClient::PostRender` as the viewport lifecycle hook;
- keep the model and request layer independent of the renderer;
- use plugin-owned presentation;
- if a cooked UI PAK is added later, replace the Canvas presenter with a
  plugin-owned UMG presenter.

## Current issues

### Layout

- The model used an 876×720 frame while projection used 780×720.
- The mismatch caused horizontal overflow and offset hit targets.
- Rendering was capped at 1.0 scale, so 1440p and 4K stayed near 720p size.
- Keep fit-by-height/width scaling with letterboxing. Do not stretch X alone.

### Render cost

Every Canvas primitive goes through reflected Unreal calls.

Known costs:

- reflected Canvas functions were rediscovered too often;
- the old soft surface used three overlapping rectangles;
- bordered panels used six calls;
- immediate-mode command generation happens every frame;
- filtering caches do not reduce primitive submission.

Icon loading is already bounded to one new asset per frame. Missing icons are
negatively cached.

### Persistence and cache files

- Settings: `plugins/RuneSchema.Helpy/settings/settings.jsonc`
- Favorites: `runtime/live/saved/references/Helpy-favorites.json`
- Reference inputs: `runtime/live/saved/references/Helpy-*.json`
- Diagnostic catalog cache: `runtime/live/saved/cache/Helpy-catalog-cache.json`
- Category snapshots: `Helpy-catalog-{items,ai,npcs,resources,...}.json`

The file separation is fine. The missing piece is cache identity: game build,
RuneSchema version, mounted mods, and source fingerprint should be part of the
same snapshot contract.

Preference writes should also leave the render path.

## First pass

Implemented:

- one 876×720 logical coordinate system for layout, render, and hit testing;
- viewport fitting with scale up to 1.35×;
- cached Canvas `UFunction` lookups;
- flat one-fill surfaces;
- two-call bordered panels.

Command behavior, validation, catalog semantics, and input safety remain
unchanged.

## Target presentation

Use four stable regions:

- 64 px command bar;
- optional 168 px section rail;
- fluid workspace;
- 48 px status/action bar.

Use shared components for commands, fields, segmented controls, cards, rows,
status callouts, modals, and pagination.

Color should show state, not page identity.

## Target rendering

1. Keep `PostRender` as the lifecycle boundary.
2. Rebuild a retained scene only when model revision, viewport class, or hover
   state changes.
3. Separate static shell commands from dynamic content.
4. Prefer flat fills and text.
5. Record primitive count and render duration in debug builds.
6. Target **180 Canvas calls or fewer** on a normal browse page.
7. If Canvas remains too expensive, move the same scene model to plugin-owned
   UMG.

## Persistence classes

Keep persisted data explicit:

- **preferences** — hotkey, favorites, density, last section;
- **session** — optional draft/navigation recovery;
- **catalog** — immutable generated snapshot;
- **references** — user-requested discovery inputs.

Prepare writes outside the render callback. Publish with atomic replacement and
coalesce repeated writes by generation.

## Catalog identity

Each snapshot should include:

```json
{
  "schema": 2,
  "gameBuild": "...",
  "runeSchemaVersion": "...",
  "sourceFingerprint": "...",
  "mountedModsFingerprint": "...",
  "generatedUnixSeconds": 0,
  "categories": {}
}
```

On open:

1. show a valid matching snapshot immediately;
2. revalidate in the background;
3. reject mismatched schema/game identity;
4. keep the old file for diagnostics;
5. publish one immutable typed snapshot in memory.

Do not parse catalog JSON in `PostRender`.

## Delivery order

1. Measure Canvas calls and frame time at 720p, 1080p, 1440p, ultrawide, and 4K.
2. Finish the shared responsive shell.
3. Move catalog decode and preference writes off the render callback.
4. Add versioned cache identity.
5. Rework item, NPC/AI, resource, details, clone, recipe, and journal flows.
6. Re-evaluate UMG only after profiling the optimized Canvas path.

## Acceptance

- no clipped controls at the target viewport sizes;
- hit boxes match visuals at every scale;
- valid cache opens without a full scan;
- search/filter/page changes perform no file I/O or catalog parse;
- favorites survive clean shutdown;
- normal browse pages stay within the primitive budget;
- input is restored after close, focus loss, world travel, and renderer failure.
