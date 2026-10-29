# RuneSchema and Helpy 0.8.0.3

October 28, 2026. Changes since the refreshed [0.8.0.2 release](RELEASE-0.8.0.2.md).
See [installation, defaults and verification](RELEASE-0.8.0.3.md).

## Save cleanup

- Make cleanup of the selected character/world at entry the default for new settings and configurations without an explicit scope.
- Keep character selection and preview outside RuneSchema's cleanup boundary.
- Retain **Blanket save cleanup (fallback)** as a saved, opt-in setting across supported local saves.
- Add **Next character/world entry** and **Next game launch** activation choices. Saving a choice does not start a background sweep.
- Preserve the session's applied policy when changes activate at the next launch.
- Read the selected local character's name and GUID through native getters and verify the GUID before touching its exact file.
- Reuse verified full-file backups and atomic replacement. Ambiguous selection or identity mismatch does not authorize a broader sweep.
- Clean local world files only at the verified native slot/read boundary; do not sweep client world files for network joins.
- Recognize the native character-load RPC after its cheap name check and require gameplay context for the payload adapter.
- Keep Xbox WGS provider-owned and server character cleanup on incoming payloads rather than local client folders.
- Retain section-specific registry checks, supported cleanup limits and deferred world-dependent quest/journal handling.

## Quick Settings and Mod Menu

- Open Quick Settings as a centered, scrollable panel with a visible Close button.
- Group cache, performance and game launch/save controls to reduce clutter.
- Add RuneSchema's optional page for Maxxfilth's Mod Menu through a root `modmenu.json`.
- Seed missing exposed settings before Lua startup through the validated writer and an original-file backup; preserve explicit choices.
- Present enabled nested Lua packages as complete nodes in UE4SS's directory listing for the current runtime.
- Let Mod Menu find Bootylicious's **Morph Sliders** while scripts, metadata, artwork and configs retain their original paths.
- Preserve sibling entries, native return values and native errors. Reject stale manifests; omit disabled and unsupported packages.
- Use stock Lua-start callbacks, including the legacy overload. Add no polling, filesystem projections or Mod Menu/UE4SS replacement.
- Keep native/hybrid DLL packages outside the companion loader; install them through stock UE4SS.

## Helpy performance and browsing

- Retain unchanged immutable source indexes so refreshes do not retire large duplicate trees on the game/UI thread.
- Carry bounded `ID.txt` version hints through source verification while keeping existing caches usable.
- Add optional game/server-tick and Helpy FPS readings, a refresh interval and a Helpy render limit.
- Add an optional click-through game FPS overlay with a configurable hide/show shortcut; hidden counters stop requesting readings.
- Reduce idle wakeups while retaining input wakeup, unfocused throttling and no rendering for hidden/minimized windows.
- Attribute travel, ground-query, map and player/role work through the existing bounded timing cadence.
- Clear old search, favorites-only and personal-tag filters when selecting an item family.
- Expand armor-family normalization for cowls, tunics, coats, chaps, robe tops/bottoms and medium/full helmet variants.
- Keep family handling consistent between the core and Helpy, including dyed and trimmed sets.
- Shorten the Item Bay cache button to **Cache...**.

## Maps and registries

- Allow UI artwork, the world-map image/alignment and map filters to update independently or together.
- Validate map download size/hash and reuse an unchanged image/alignment pair.
- Let explicit filter refreshes check the feed without a confirmed game-version change; retain retry limits and existing data on failure.
- Accept generic `DA_Registry_*` names alongside RuneSchema registry names and scoped cooked `Registry/` declarations.
- Allow shard names to differ from their internal owner while retaining owner, path, schema, cycle, duplicate and transaction checks.
- Keep registry discovery bounded and off per-frame paths.

## Packaging and limits

- Align branding, Windows resources, CMake versions and bundled plugin manifests to **0.8.0.3**.
- Include the Mod Menu descriptor, discovery helper, current defaults and compressed DLL pair in the clean runtime package.
- Update download links and installation/save/companion guides; retain historical releases and changelogs.
- Keep reflection mapping generation tied to executable changes and cache/artwork downloads independently updateable.

Copied Steam saves verified preview protection, selected-only pruning, backups,
stable re-entry and both activation timings. Copied Mod Menu 2.0.2 packages and
an isolated game verified directory discovery and settings. These checks do not
certify every mod, establish measured FPS gains, or replace live Game Pass WGS
and dedicated-server join testing.
