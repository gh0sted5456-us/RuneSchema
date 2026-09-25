# Raw / DataTables loader

**Folder:** `RuneSchema/mods/<ModName>/raw/`

Supported DataTable row creation and patching.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

`/raw` creates or patches supported DataTable rows.

```json
{
  "DT_WearableEquipment": {
    "mymod_legs": {
      "Defense": 20.0,
      "MagicResistance": 5.0
    }
  }
}
```

The classic map form uses the DataTable's short name. When a short name is
ambiguous or you need exact target control, use a registry-patch document with
a full cooked `target.objectPath`, row struct, ownership, preconditions, and
operation. See
[`schemas/registry-patch-v1.schema.json`](../schemas/registry-patch-v1.schema.json)
and `examples/RegistryPatch`.

Do not guess nested layouts from a USMAP alone. RuneSchema compares the target
against live reflection before committing a row.

## Simple rules

- In the classic `/raw` map form, use the DataTable's **short name** as the top-level key and the row name below it.
- RuneSchema checks the target row's live reflected structure. Supplied fields that exist are written; unknown fields are reported.
- Use a registry-patch document with `target.objectPath` when you need an exact cooked DataTable path or stricter ownership/precondition handling.

## FAQ

### FAQ-RAW-001 — Can /raw edit a DataTable that RuneSchema does not have a special loader for? {#faq-raw-001}

Usually yes, as long as RuneSchema can resolve the DataTable and the row/field
layout matches live Unreal reflection. The classic raw editor is generic: it
looks up each supplied property on the target row struct rather than requiring
a hard-coded implementation for every DataTable.

### FAQ-RAW-002 — Can I use a full cooked DataTable path as the top-level key in classic /raw JSON? {#faq-raw-002}

No. The classic map form is keyed by the DataTable's short name, such as
`DT_WearableEquipment`. When an exact path is required, use a registry-patch
target with `objectPath`, for example
`/Game/.../DT_Name.DT_Name`.

### FAQ-RAW-003 — What happens if I specify a field that does not exist on the row struct? {#faq-raw-003}

RuneSchema does not blindly write it. The loader checks the reflected row
property; an unknown field is reported instead of being treated as a valid
property.

### FAQ-RAW-004 — Do I need to provide every field in an existing row? {#faq-raw-004}

No. For an existing row, provide the fields you intend to change. RuneSchema
writes the matching reflected properties you supplied.

## Working examples

- [Capes: Attack cape override](../examples/Capes/raw/DT_ITEM_Cape_Attack_override.json)
- [Capes: Trimmed Magic skillcape](../examples/Capes/raw/DT_ITEM_Cape_Trimmed_Skillcape_Magic_override.json)
- [Great Tree: boss loot rows](../examples/GreatTree/raw/85-GreatTreeLoot.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
