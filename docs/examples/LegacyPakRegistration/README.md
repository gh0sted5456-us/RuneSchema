# Register an older PAK without recooking

This is an authoring template, not an installable gameplay mod. Replace every
example path with an inspected asset from the original mod. Item/recipe loose
registration exists in 0.8.0.1; the `Buildings` metadata in this example requires
the enrollment development build.

```text
RuneSchema/mods/
  runeschema.txt
  LegacyWeapons/
    ID.txt                 (preserve it if this mod already has one)
    paks/
      OriginalWeapons.pak  (unchanged original files)
      OriginalWeapons.utoc (only if shipped by that original mod)
      OriginalWeapons.ucas (only if shipped by that original mod)
    registry/
      10-native.jsonc
```

1. Keep the original PAK/container triplet and required dependencies intact.
   If the provider is already installed and mounted elsewhere, leave it there;
   do not install a second copy just to use this sidecar.
2. Place the supplied `registry/10-native.jsonc` under the enabled mod folder.
   Add or retain `LegacyWeapons : 1` in `mods/runeschema.txt`, without replacing
   the user's other entries. Preserve an existing mod's folder/owner and ID.txt.
3. Replace the exact ItemData, RecipeData and BuildingPieceData object paths.
   Do not rename/rebase the original assets or assign new persistence IDs.
4. Retain the recipe's existing cooked station/unlock relationships. Where
   missing, add supported recipe placement/discovery metadata; registration
   alone does not invent those relationships. `Unlock: false` grants nothing.
5. Restart; verify runtime resolution/registration, use, saving and reloading.
   For multiplayer, install the same content and sidecar on server and clients.

No `DA_RuneSchemaRegistry` asset, `RegistryOwner` property or recook is needed
for this loose route. Ownership comes from the enabled folder. Keep
`SchemaVersion: 1` and `Entries: []`; do not wrap this in `Properties` or add a
root `Enrollment` field.

Use the sidecar when the PAK lacks a declaration, or to announce missing
references. Do not ship redundant loose `/assets`, `/recipes` or `/buildings`
pointer definitions for the same content. If the author later cooks an
equivalent declaration, remove the redundant sidecar. Matching native enrollment
declarations are idempotent; conflicting explicit policies or building owners are rejected,
not resolved by guessing which copy is newer. Loose loader overrides remain a
separate, intentional mechanism for modifying settings.

An older PAK still needs compatible cooked serialization, native data classes,
valid original persistence identities, complete dependencies and actual
gameplay behavior. JSONC registration cannot repair missing or incompatible
cooked content. Back up and test existing saves before distributing a conversion.

See [registry authoring](../../reference/loaders/registry.md) and
[cooked/loose enrollment scope](../../COOKED-PAK-REGISTRY-MANIFEST.md#enrollment-scope).
