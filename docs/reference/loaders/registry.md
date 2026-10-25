# Registry loader

## Step-by-step workflow

Examples with placeholder paths/IDs are templates. Use inspected values and the
[shared testing checklist](../../modders/testing-and-compatibility.md) before sharing.

1. Determine whether you need a presentation/authority declaration or native-reference enrollment.
2. Create `registry/10-camp-registry.jsonc` using the matching example below and unique owned IDs.
3. Replace all cooked paths. Mark schema-created references explicitly and construct them through their owning loader first; avoid redundant registration.
4. Restart and inspect class/identity/authority validation and client assets. A declaration cannot supply a missing object or grant permission.

**Tie-ins:** [Cooked declarations](../../COOKED-PAK-REGISTRY-MANIFEST.md), [assets](assets.md), [recipes](recipes.md), [journal](journal.md) and [quests](quests.md).


**Folder:** `RuneSchema/mods/<ModName>/registry/`

Multiplayer server-authority and client-presentation declarations.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Registry files merge client presentation and server authority declarations
under the owning mod namespace.

```jsonc
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

For the complete lane/status table and the development cooked building/menu
extension, see [enrollment scope](../../COOKED-PAK-REGISTRY-MANIFEST.md#enrollment-scope).
The loose and cooked routes use the same parser; do not invent an `Enrollment`
root field or assume declaration acceptance means the native consumer works.

### Register a cooked item without cooking a registry declaration

For an unchanged older PAK with several content types, use the
[legacy PAK sidecar walkthrough and JSONC starter](../../examples/LegacyPakRegistration/README.md).
The same loose document can announce items and recipes; development builds
also accept cooked building enrollment/menu metadata without a registry asset
inside the PAK. No shared bridge inheritance is required for a plain sidecar.

You can ship the declaration as loose JSON or JSONC instead of recooking the
PAK just to add an `RSREG_` or `DA_RuneSchemaRegistry` asset. RuneSchema reads
the enabled mod's `registry` folder and submits the referenced items to the
supported native enrollment path. This uses the same declaration contract as
a cooked registry asset; it is not a whole-PAK search.

**No cooked registry declaration is required. The referenced item still must
exist.** JSON does not manufacture a missing ItemData, mesh, icon or Blueprint.
For an item created from an existing compatible item entirely through JSONC,
use the [`assets` clone workflow](assets.md) instead; that loader already
registers ordinary item clones.

1. Put the cooked item provider in the mod's `paks` folder, or install its
   required provider separately. Inspect the exact ItemData object path.
2. Put the following file at
   `RuneSchema/mods/GoldenArsenal/registry/10-native-items.jsonc`.
3. Add or retain `GoldenArsenal : 1` in
   `RuneSchema/mods/runeschema.txt`. Keep required providers enabled and before
   dependent mods; do not replace the rest of the user's mod list.
4. Replace the example path, restart the game, and verify the live item and its
   identity. A cached card is not proof of successful runtime enrollment.

```text
RuneSchema/mods/
├── runeschema.txt
└── GoldenArsenal/
    ├── paks/
    │   └── GoldenArsenal.pak
    └── registry/
        └── 10-native-items.jsonc
```

```jsonc
{
  "SchemaVersion": 1,
  // Keep Entries even when there are no presentation/authority declarations.
  "Entries": [],
  "NativeRegistries": {
    "Items": [
      // Replace this with the full object path from your installed content.
      "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear"
    ]
  }
}
```

[Download the JSONC starter](../../examples/LooseRegistryWalkthrough/GoldenArsenal/registry/10-native-items.jsonc).
For strict `.json`, remove the comments; keep the same document fields. The
owner comes from the enabled mod folder, so do not add a root `RegistryOwner`
field to this loose document. `RegistryOwner` is a property of the alternative
cooked declaration asset.

The item must already have the right native class, a unique `InternalName`,
and a valid, stable `PersistenceID`. Do not put identity overrides inside
`NativeRegistries.Items`: it contains references, not item definitions. Use a
full object path (`/Root/Folder/Asset.Asset`), not a disk path, `.pak` filename,
display name, package-only path, or mesh reference in place of ItemData.

To declare several items, add their exact paths to the same `Items` array.
Do not also declare the same catalogue through a second cooked declaration or
redundant empty `assets` files. Choose one enrollment route for each reference.
Add recipe, station, vendor and discovery relationships separately: registering
an item does not make it craftable or grant it to a player.

#### Verify and troubleshoot

- With verbose logging enabled, `[NATIVE-REGISTRY][DECLARED]` identifies the
  owner and source document accepted for enrollment. It does **not** certify
  that every live identity registered successfully.
- `Registry file ... rejected` means the declaration failed validation. Check
  `SchemaVersion`, the required `Entries` array, exact field spelling, duplicate
  paths, supported lanes, and valid object paths.
- If the item cannot resolve, check the enabled mod, mounted provider, exact
  path and actual native class. Do not assign another persistence ID just to
  make an error disappear.
- Verify delivery/use and save/reload with a disposable character before
  release. Restart after registry changes; registry auto-reload explicitly
  requests a restart. An unresolved declaration is not evidence that saved
  items should be deleted.

See [cooked and loose declaration alternatives](../../COOKED-PAK-REGISTRY-MANIFEST.md)
for recipes, journal/lore metadata and the lane-specific limits.

### Native registration from a schema mod

Enabled loose `registry/*.jsonc` files accept the same
`NativeRegistries` lanes as cooked `RSREG_` declarations: Items, Recipes,
Quests, CombatSpells, UtilitySpells, EquipmentEffects, Journals, Lore,
WearableMeshData, MeleeAttackClasses and RangedAttackClasses. The metadata-only
`CacheAssets` field is also accepted in both formats for exact original artwork
ownership. It does not register textures as gameplay items. The PAK container
is not a prerequisite for announcing these references.

The eight cooked native-data-family lanes also use this route:
`ContainerContents`, `FarmPlants`, `Trees`, `TreeGrowth`,
`FixedTextNotifications`, `FTUENotifications`, `PlayerHooks` and `WorldHooks`.
Their [consumer-lifecycle limits](../../COOKED-NATIVE-DATA-FAMILIES.md) still
apply. Development `Buildings` declarations likewise work from JSONC; they are
not restricted to embedded PAK manifests.

Development-only additions: `Skills` and `SkillPerks` are declaration-only,
explicitly blocked native adapters, not working custom progression support.
`Enums` read-only verifies exact mod-owned cooked `UserDefinedEnum` assets and
includes stable symbol/value fingerprints in owner/network agreement. All
three require full `/Owner/.../Asset.Asset` or `/Game/Mods/Owner/.../Asset.Asset`
paths matching the declaration owner; `Source: Schema` is not supported.
Ownership/type errors are reported, never interpreted as permission to modify
native entries. A cooked mod enum cannot substitute for native `ESkill`.

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
