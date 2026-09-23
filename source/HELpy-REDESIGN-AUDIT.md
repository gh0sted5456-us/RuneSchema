# Helpy renderer, persistence, and cache redesign

## Decision

Keep Helpy attached to the real game viewport, but do not attempt to reuse a
Dragonwilds game screen. The current plugin already hooks
`UGameViewportClient::PostRender`, captures the local player controller, and
projects its own interface into the active viewport. Re-skinning or cloning a
game UMG screen would bind Helpy to private widget classes, cooked assets,
focus rules, and game-version-specific object paths. It is less work for a
prototype and more work for every subsequent game update.

The recommended production design is a plugin-owned responsive presentation
layer on the existing viewport lifecycle. The model and request boundary can
remain engine-independent. If the release pipeline later accepts a dedicated
cooked UI pak, replace only the Canvas presenter with a plugin-owned UMG
presenter; do not bind to a game-owned menu.

## Findings

### Viewport and input

- The model laid out a 876 x 720 frame while the projector centred and scaled
  it as 780 x 720. This caused horizontal overflow, offset hit targets, and
  inconsistent behaviour between aspect ratios.
- Rendering was capped at 1.0 scale. The interface therefore remained at its
  720p logical size on 1440p and 4K viewports.
- Aspect ratio handling should remain fit-by-height/width with letterboxing.
  Stretching the X axis independently would make controls inconsistent on
  ultrawide displays.

### Render cost

- Every rectangle, text run, and icon invokes a reflected Unreal Canvas
  function through `ProcessEvent`.
- Reflected Canvas functions were rediscovered for every primitive.
- The rounded-looking `soft` surface used three overlapping rectangles. A
  bordered panel used six, and most buttons nested two such surfaces.
- The UI builds an immediate command list each frame. Its filtering caches are
  useful, but they do not reduce Canvas primitive submission cost.
- Icon lookup is already sensibly throttled to one new asset per frame and
  weakly cached. Missing icons are negatively cached.

### Current persistence and cache topology

- Plugin settings: `plugins/RuneSchema.Helpy/settings/settings.jsonc`.
- Favorites: `runtime/live/saved/references/Helpy-favorites.json`, with legacy
  migration and atomic validated writes.
- Reference/index inputs: `runtime/live/saved/references/Helpy-*.json`.
- Diagnostic catalog cache:
  `runtime/live/saved/cache/Helpy-catalog-cache.json`; it is used only under
  the advanced/full-scan policy.
- Tool catalog snapshots: `Helpy-catalog-{items,ai,npcs,resources,...}.json`.
- The plugin receives catalog revisions from core, then copies JSON and
  reconstructs typed vectors on the game/render thread when a revision changes.

The files are separated safely, but the cache does not yet have one explicit
identity tying it to game build, RuneSchema schema, mounted mod set, and source
fingerprint. Preferences also save synchronously from the render path.

## Implemented first pass

- Unified layout, rendering, and hit testing on one 876 x 720 coordinate space.
- Fit the complete frame to the viewport and allow scaling up to 1.35x.
- Cache reflected Canvas `UFunction` objects outside the primitive hot path.
- Replaced faux-rounded three-call surfaces with a consistent flat component
  language using one fill; bordered panels now use two calls instead of six.

This pass preserves commands, validation, catalog semantics, and input safety.

## Target architecture

### Presentation

Use a shell with four stable regions: 64 px command bar, optional 168 px
section rail, fluid workspace, and 48 px status/action bar. Derive columns from
available logical width instead of hard-coding four cards. Define compact,
regular, and wide breakpoints and keep minimum 40 px pointer targets.

All pages should use the same primitives: command, field, segmented control,
card, table row, status callout, modal, and pager. Color must communicate only
state (active, warning, destructive, unavailable), not page identity.

### Rendering

1. Keep `PostRender` only as the viewport lifecycle boundary.
2. Build a retained `Scene` when model revision, viewport class, or pointer
   hover state changes.
3. Separate static shell commands from dynamic content commands.
4. Submit flat fills and text; reserve multi-pass decoration for focused or
   destructive controls.
5. Record primitive count and render duration in debug builds. Enforce a
   normal-page budget (recommended: <= 180 Canvas calls).
6. If a cooked plugin UI pak becomes acceptable, implement the same scene and
   command contracts in a plugin-owned UMG widget and retire primitive Canvas
   submission. Avoid a game-owned widget in either case.

### Persistence

Split persisted data into explicit classes:

- `preferences`: hotkey, favorites, density, last section; tiny, durable,
  debounced, and atomically replaced.
- `session`: open draft and navigation state; optional, crash-recovery only,
  never treated as authoritative game data.
- `catalog`: immutable snapshot plus metadata; replace as a generation, never
  patch the published file in place.
- `references`: user/requested discovery inputs; durable until explicitly
  replaced.

Writes should be prepared off the render callback, published through the
existing staging/readback pattern, and coalesced by generation. Shutdown should
flush preferences with a bounded wait.

### Catalog cache contract

Each snapshot should contain:

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

On open, display a valid matching snapshot immediately, then revalidate in the
background (stale-while-revalidate). Reject mismatched schema/game identities;
retain the old file for diagnostics rather than merging it. Publish one typed,
immutable in-memory snapshot and let category views share it. Filtering should
use pre-normalized search text and stable IDs; JSON parsing and typed-vector
construction must not occur in `PostRender`.

## Delivery order

1. Measure Canvas primitive count and frame time; establish responsive layout
   fixtures at 1280x720, 1920x1080, 2560x1440, 3440x1440, and 3840x2160.
2. Introduce design tokens and responsive shell/components without changing
   command semantics.
3. Move catalog JSON decoding and preference writes off the render callback;
   publish immutable snapshots by revision.
4. Add versioned cache identity and stale-while-revalidate startup.
5. Redesign the item, NPC/AI, resource, details, clone, recipe, and journal
   flows against the shared components.
6. Evaluate plugin-owned UMG only after profiling the optimized Canvas path on
   target Steam/GOG and Game Pass builds.

## Acceptance gates

- No clipped or off-centre controls at the five viewport fixtures.
- Pointer hit boxes match visuals at every scale.
- Opening with a valid cache shows browseable content without a full scan.
- Search/filter/page changes perform no file IO and no catalog JSON parse.
- Favorite changes survive a clean shutdown and never write once per frame.
- Typical browse pages stay within the primitive budget and do not materially
  reduce game frame rate.
- Steam/GOG and Game Pass builds preserve input restoration on close, focus
  loss, world travel, and renderer failure.
