# Raw / DataTables loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/raw/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Resolve the exact DataTable and inspect the live row struct; prefer a full path when short names are ambiguous.
2. Create `raw/10-camp-rows.jsonc` with table, row and only the fields being changed.
3. Replace all mock-up field names/types. Distinguish patching an existing row from constructing a new row with required data.
4. Restart and inspect rejected properties/read-back, then exercise the asset or station that consumes the row.

**Tie-ins:** [Assets](assets.md) references row handles; [buildings](buildings.md) and [recipes](recipes.md) connect station data.


**Folder:** `RuneSchema/mods/<ModName>/raw/`

Supported DataTable row creation and patching.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

`/raw` creates or patches supported DataTable rows.

```jsonc
{
  "DT_WearableEquipment": {
    "mymod_legs": {
      "Defense": 20.0,
      "MagicResistance": 5.0
    }
  }
}
```

The top-level key may be a unique short name or the exact cooked DataTable
object path:

```jsonc
{
  "/Game/MyMod/Data/DT_Custom.DT_Custom": {
    "ExistingRow": {"SomeField": 10},
    "NewRow": {"SomeField": 20}
  }
}
```

Use the exact path for custom tables or whenever a short name could be
ambiguous. A registry-patch document adds stricter ownership, preconditions,
and all-or-nothing row operations. See
[`schemas/registry-patch-v1.schema.json`](../../schemas/registry-patch-v1.schema.json)
and `examples/RegistryPatch`.

Qualified targets are resolved by their exact cooked object path and the live
object must report that same path. Composite parents are expanded only when
their `_Common` parent is unambiguous. Applying a target reserves its row range
before callbacks, preventing a serialization/reentry callback from applying
the same range twice. Short names retain legacy behavior; qualify custom or
conflicting tables instead of depending on traversal order.

Do not guess nested layouts from a USMAP alone. RuneSchema compares the target
against live reflection before committing a row.

## Simple rules

- Use a unique DataTable short name or an exact cooked object path as the top-level key and the row name below it.
- RuneSchema checks the target row's live reflected structure. Supplied fields that exist are written; unknown fields are reported.
- Use a registry-patch document with `target.objectPath` when you need an exact cooked DataTable path or stricter ownership/precondition handling.

## FAQ

### FAQ-RAW-001 — Can /raw edit a DataTable that RuneSchema does not have a special loader for? {#faq-raw-001}

Usually yes, as long as RuneSchema can resolve the DataTable and the row/field
layout matches live Unreal reflection. The classic raw editor is generic: it
looks up each supplied property on the target row struct rather than requiring
a hard-coded implementation for every DataTable.

### FAQ-RAW-002 — Can I use a full cooked DataTable path as the top-level key? {#faq-raw-002}

Yes. Use `/Game/.../DT_Name.DT_Name`. Exact paths are recommended for modded
tables and ambiguous short names.

### FAQ-RAW-003 — What happens if I specify a field that does not exist on the row struct? {#faq-raw-003}

RuneSchema does not blindly write it. The loader checks the reflected row
property; an unknown field is reported instead of being treated as a valid
property.

### FAQ-RAW-004 — Do I need to provide every field in an existing row? {#faq-raw-004}

No. For an existing row, provide the fields you intend to change. RuneSchema
writes the matching reflected properties you supplied.

## Working examples

- [Capes: Attack cape override](../../examples/Capes/raw/DT_ITEM_Cape_Attack_override.json)
- [Capes: Trimmed Magic skillcape](../../examples/Capes/raw/DT_ITEM_Cape_Trimmed_Skillcape_Magic_override.json)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
