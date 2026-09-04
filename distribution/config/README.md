# RuneSchema 0.6.1E settings

This clean configuration keeps deterministic mod management and compatibility
reporting enabled while leaving runtime-risk and developer-noise switches off.

- `languageOverride`: empty uses the game's active language.
- `enableAutoReload`: reload changed mod files while running; disabled by default.
- `enableDebugLogging`: verbose troubleshooting output; disabled by default.
- `enableExperimentalDropScaling`: permits `/spawns` `DropIncreasePercent`; disabled
  until the mod author deliberately opts in.
- `tooling.enabled`: master authoring-tools switch.
- `enableSchemaGeneration`: writes current JSON Schemas for supported loaders.
- `enableFModelSnippetGenerator`: generates configured FModel snippets; opt-in.
- `modsTxt`: creates and reconciles the authoritative deterministic load-order list,
  preserves comments, and accepts only `0` or `1` states.
- `compatibilityReports`: records competing target/property/array writes so load-order
  behavior is visible to authors.

RuneSchema was created by Snorkles. Extended 0.6.1E features are by
Jonesing4Space. RuneSchema is based on Okaetsu's PalSchema.
