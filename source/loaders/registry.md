# Registry loader

**Folder:** `RuneSchema/mods/<ModName>/registry/`

Multiplayer server-authority and client-presentation declarations.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Registry files merge client presentation and server authority declarations
under the owning mod namespace.

```json
{
  "SchemaVersion": 1,
  "Entries": [{
    "Id": "spell_presentation",
    "Kind": "SpellPresentation",
    "Spell": "/Game/MyMod/Spells/DA_MySpell.DA_MySpell",
    "Presentation": [{
      "Phase": "SpawnVFX",
      "Class": "/Game/MyMod/VFX/BP_MyImpact.BP_MyImpact_C",
      "Classification": "PureVFX"
    }]
  }]
}
```

Duplicate `ModName:Id` keys are rejected and reported. Presentation assets must
exist on clients. Authority actions are validated on the server; the JSON does
not grant permission by itself.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
