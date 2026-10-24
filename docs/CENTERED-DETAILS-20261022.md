# Centered details and bounded wrapping

The shared details renderer centers each wrapped text line inside the actual available content width, including disabled/status text and preserved paragraph breaks. It reuses the existing Home centered-line design rather than changing ImGui's global text layout. Tooltip boxes remain centered on their hovered control and constrained to the viewport; long content retains scrolling.

Quick Settings, Save Bypass hints, runtime, loading, profiles, mod metadata/health, recipes, save-item hints, item hover descriptions and resource/map hover details use the shared renderer. Compact spawn hover cards center their icon and text; the regular placement editor is unchanged. Stat tables and editable fields retain their readable column alignment.

The map Spawn / Build / Teleport action window is centered in Helpy's viewport instead of anchored at the right-click position. Its size remains bounded and stable; the action footer and selectors keep their existing behavior.

The older hint-specific wrap widths could exceed a 300-pixel tooltip and cut text off horizontally. Details text now ignores those wider local wrap settings and wraps to the current window's usable width instead. Regression checks cover centered final lines, cursor restoration, secondary colors, long unbroken text staying within horizontal bounds, and map-window centering/stability at compact viewport sizes.

This UI follow-up is separate from the already-installed icon-manifest/mapping-reuse correction. Do not claim it is installed until its own installation receipt exists.
