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

## Simple rules

- Registry entries join server-authority declarations with client presentation under the owning mod namespace.
- Every `ModName:Id` identity must be unique.
- Presentation assets must exist on clients; an authority declaration is still validated on the server.

## FAQ

### FAQ-REGISTRY-001 — Does a registry declaration grant server permission by itself? {#faq-registry-001}

No. Authority actions are validated on the server. Declaring an entry does not
bypass that validation.

### FAQ-REGISTRY-002 — Do clients need the assets referenced by presentation entries? {#faq-registry-002}

Yes. Client presentation assets must be installed wherever they are rendered.

### FAQ-REGISTRY-003 — What happens if two entries use the same ModName:Id? {#faq-registry-003}

The duplicate identity is rejected and reported.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
