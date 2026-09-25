# RuneSchema registry patching (0.7.5.24)

RuneSchema keeps the original `/raw` table format for compatibility and adds two strict, opt-in definition folders beneath each enabled mod:

```text
RuneSchema/mods/<mod-id>/raw/patches/*.json[c]
RuneSchema/mods/<mod-id>/raw/character_customization/*.json[c]
```

Files are parsed in ordinal path order. They are limited to 2 MiB, duplicate JSON keys are rejected, unknown schema fields are rejected, and `modId` must match the containing mod folder. One bad file is isolated and reported without disabling the legacy loader or definitions belonging to other mods.

## Generic DataTable patches

Use schema `runeschema.registry-patch/v1`. Full cooked object paths are preferred. A short table name is accepted only when it resolves to exactly one loaded table after `searchRoots` and `expectedRowStruct` filters.

The default `dragonwilds.dataTableOwnedRows.v1` profile permits `addRow`, `copyRow`, `upsertOwnedRow`, and `mergeOwnedRow`. Existing vanilla rows are not writable through that profile. The elevated `dragonwilds.dataTablePatch.v1` profile permits `patchExistingRow` only with `expectedRowStruct` and reflected `requiredProperties` preconditions.

Supported tagged values are `softObject`, `softClass`, `enum`, `name`, `text`, and `dataTableRowHandle`. A same-mod dependency can use `{ "$ref": "patch:<id>#rowName" }`. Unsupported targets and destructive operations such as row deletion are rejected before mutation.

Rows created by the engine are owned by the canonical mod and patch identity. A mod cannot silently replace a vanilla row or a row owned by another mod. Transaction rows are prepared and reflected-property checked before any row in that table transaction is committed.

## Character customization

Use schema `runeschema.character-customization/v1`. Version 1 supports cooked player hair. One semantic entry expands deterministically into owned hair-zone rows and a hair-preset row, using `RS_HZ_<hash>` and `RS_HP_<hash>` names. Zone style indexes are allocated from the lowest free non-negative value at runtime. Hidden entries register the rows without requesting a menu entry.

Assets still have to be cooked and mounted by the mod. RuneSchema does not import, rig, cook, or rewrite source meshes and materials at runtime. A missing table, changed row struct, missing reflected property, ambiguous table name, invalid path, or ownership conflict fails closed with a `[REGISTRY-PATCH]` diagnostic.

See `examples/RegistryPatch` and `examples/CharacterCustomization` for authoring examples. The schemas document the file shape; runtime reflection remains authoritative for Unreal property compatibility.
