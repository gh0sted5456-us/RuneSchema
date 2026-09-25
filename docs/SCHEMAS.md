# JSON schemas

RuneSchema ships machine-readable schemas for strict authoring surfaces. Runtime reflection remains authoritative for Unreal type compatibility.

| Schema | Purpose |
|---|---|
| [`asset-patch-v2.schema.json`](schemas/asset-patch-v2.schema.json) | Asset patch definitions |
| [`character-customization-v1.schema.json`](schemas/character-customization-v1.schema.json) | Character customization definitions |
| [`registry-patch-v1.schema.json`](schemas/registry-patch-v1.schema.json) | Transactional registry and DataTable patches |

## Validation expectations

- Unknown schema fields are rejected where the strict schema applies.
- Duplicate JSON keys are rejected.
- Target resolution must be unambiguous.
- Reflected properties and row structures are checked before mutation.
- Ownership conflicts fail closed instead of replacing another mod's data silently.
