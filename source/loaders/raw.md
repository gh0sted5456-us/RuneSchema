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

Use an exact DataTable object path when a short table name is ambiguous. New
registry-patch documents can declare the target path, row struct, ownership,
preconditions, and operation. See
[`schemas/registry-patch-v1.schema.json`](../schemas/registry-patch-v1.schema.json)
and `examples/RegistryPatch`.

Do not guess nested layouts from a USMAP alone. RuneSchema compares the target
against live reflection before committing a row.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
