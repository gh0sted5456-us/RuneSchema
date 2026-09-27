# JSON schemas

RuneSchema ships machine-readable schemas for editor completion and structural
checks. The loaded game remains the final authority on whether an object, row,
field, and value type are available.

| Schema | Purpose |
|---|---|
| [`asset-patch-v2.schema.json`](schemas/asset-patch-v2.schema.json) | Direct DataAsset and class-default changes |
| [`character-customization-v1.schema.json`](schemas/character-customization-v1.schema.json) | Character customization definitions |
| [`registry-patch-v1.schema.json`](schemas/registry-patch-v1.schema.json) | Controlled registry and DataTable changes |

Current loader schemas are available for `assets`, `blueprints`,
`buildings`, `courses`, `dialogue`, `effects`, `enums`, `equipment`, `events`,
`journal`, `lore`, `nameplates`, `niagara`, `npc`, `players`, `quests`, `raw`,
`recipes`, `spawns`, `strings`, and `vendors`. They include station placement,
building fuel controls, recipe identity, equipment effects and every other
structurally checked authoring field in the current DLL. The Pages build keeps
these generated files and adds the three focused schemas above.

## Validation expectations

- Unknown schema fields are rejected where the strict schema applies.
- Duplicate JSON keys are rejected.
- Target resolution must be unambiguous.
- The loaded object's fields and row layout are checked before a change.
- Ownership conflicts fail closed instead of replacing another mod's data silently.
