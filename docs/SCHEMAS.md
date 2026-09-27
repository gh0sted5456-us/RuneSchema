# JSON schemas

RuneSchema ships machine-readable schemas for strict authoring surfaces. Runtime reflection remains authoritative for Unreal type compatibility. A successful build also exports the complete loader set to `docs/schemas/*.schema.json`; these files match the compiled loader contracts instead of being hand-maintained copies.

| Schema | Purpose |
|---|---|
| [`asset-patch-v2.schema.json`](../source/schemas/asset-patch-v2.schema.json) | Asset patch definitions |
| [`character-customization-v1.schema.json`](../source/schemas/character-customization-v1.schema.json) | Character customization definitions |
| [`registry-patch-v1.schema.json`](../source/schemas/registry-patch-v1.schema.json) | Transactional registry and DataTable patches |

Generated loader schemas are available for `assets`, `blueprints`,
`buildings`, `courses`, `dialogue`, `effects`, `enums`, `equipment`, `events`,
`journal`, `lore`, `nameplates`, `niagara`, `npc`, `players`, `quests`, `raw`,
`recipes`, `spawns`, `strings`, and `vendors`. They include station placement,
building fuel controls, recipe identity, equipment effects and every other
structurally validated authoring field in the current DLL.

## Validation expectations

- Unknown schema fields are rejected where the strict schema applies.
- Duplicate JSON keys are rejected.
- Target resolution must be unambiguous.
- Reflected properties and row structures are checked before mutation.
- Ownership conflicts fail closed instead of replacing another mod's data silently.
