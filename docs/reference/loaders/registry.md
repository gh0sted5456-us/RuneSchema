# Registry loader

**Folder:** `RuneSchema/mods/<ModName>/registry/`

Multiplayer server-authority and client-presentation declarations.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

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

### Native registration from a schema mod

Enabled loose `registry/*.jsonc` files now accept the same eleven
`NativeRegistries` lanes as cooked `RSREG_` declarations: Items, Recipes,
Quests, CombatSpells, UtilitySpells, EquipmentEffects, Journals, Lore,
WearableMeshData, MeleeAttackClasses and RangedAttackClasses. The PAK container
is not a prerequisite for announcing these references.

```jsonc
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": [{"Path": "/Game/RuneSchema/MyMod/Items/MyCape.MyCape", "Source": "Schema"}],
    "WearableMeshData": ["/Game/Existing/MeshData/DA_Cape.DA_Cape"]
  }
}
```

Use real full object paths, not the example placeholders. The owning `/assets`,
`/recipes`, `/quests`, `/journal` or `/lore` loader constructs schema-native
entries first; declaration alone does not construct an arbitrary Unreal class.
Ordinary schema items/recipes/journals already register through their owning
loaders, so do not add redundant declarations unless another integration needs
them. Mark a schema-created reference with `{"Path":"/...","Source":"Schema"}`;
a plain string remains an existing-asset reference. This is explicit metadata,
not a guess based on a `/Game/RuneSchema/` namespace that a PAK might also use.
Schema recipe/journal references are not converted into duplicate cooked-style
definitions. Runtime journal declarations must
resolve to an identity already registered by the journal/lore service.

`LoaderDefinitions.Recipes`, `.Journals` and `.Lore` are also accepted from a
loose registry document for existing cooked references, with the same bounded
metadata validation. Keep construction, patches and clones in the appropriate
ordinary loader folder; do not repeat a reference marked `Source: Schema` in
`LoaderDefinitions`. Duplicate or contradictory cooked policies remain errors.

Every native lane still validates its target class and existing identity rules.
Required-lane rejection blocks cleanup certification; it does not make the
declared IDs orphaned. No schema declaration bypasses authority checks, enables
a disabled mod, or supplies missing Blueprint/mesh content. Native enrollment
is startup work, not an invitation to rewrite sealed multiplayer IDs mid-world.

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

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
