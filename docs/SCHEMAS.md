# JSON schemas

Use RuneSchema’s schemas for editor suggestions and quick checks while you
write your mod. Then check the target in your game: its current objects, rows and
field types determine which changes it can use.

| Schema | Purpose |
|---|---|
| [`asset-patch-v2.schema.json`](schemas/asset-patch-v2.schema.json) | Direct DataAsset and class-default changes |
| [`character-customization-v1.schema.json`](schemas/character-customization-v1.schema.json) | Character customization definitions |
| [`registry-patch-v1.schema.json`](schemas/registry-patch-v1.schema.json) | Controlled registry and DataTable changes |

Current loader schemas are available for `assets`, `blueprints`,
`buildings`, `courses`, `dialogue`, `effects`, `enums`, `equipment`, `events`,
`journal`, `lore`, `nameplates`, `niagara`, `npc`, `players`, `quests`, `raw`,
`recipes`, `registry`, `spawns`, `strings`, and `vendors`. They describe their
supported document shapes, including station placement, building fuel controls,
recipe identity and equipment effects. Reflected game properties are not an
exhaustive schema field database. The Pages build keeps
these generated files and adds the three focused schemas above.

## Check your definitions {#validation-expectations}

- Unknown schema fields are rejected where the strict schema applies.
- Give each key one value. Duplicate keys are rejected by strict readers and can behave differently in older readers.
- Target resolution must be unambiguous.
- The loaded object's fields and row layout are checked before a change.
- Strict registry ownership conflicts reject the registration. Ordinary field overrides can intentionally replace an earlier mod's value; use a small compatibility patch and document its order.

A schema check does not resolve a cooked object, confirm a native property or
prove gameplay/save compatibility. Legacy reflection loaders can report an
unknown property while applying other supported fields. Per-array staged edits
are not a transaction across every file in a mod. Follow the
[operator guide](modders/append-and-targeted-patches.md) and
[testing checklist](modders/testing-and-compatibility.md).
