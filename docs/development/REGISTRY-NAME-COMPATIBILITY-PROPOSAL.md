# Registry names and module registry folders

Status: implemented, checked and installed with the combined maintenance build
on Steam, Game Pass and the dedicated server on October 27, 2026.

The Ritualist failure showed an avoidable naming dependency: a declaration owned
by the internal module `Conjurer` referenced `RSREG_Ritualist_*` children. The
previous contract rejected those names before inspecting their declarations.
The package was corrected separately; the shared loader now permits these names
without weakening schema, owner or shard-transaction checks.

## Cooked declarations

- Preserve `DA_RuneSchemaRegistry_*` discovery and add `DA_Registry_*` aliases.
  Keep the existing name convention as a cheap discovery hint, not ownership
  evidence.
- Include a mod's cooked `Registry/` directory in scoped discovery. A cooked
  asset in this directory still comes from its owning mounted PAK; it is not a
  loose JSONC file.
- Confirm a reflected registry string property, supported schema and explicit
  internal owner before accepting a declaration. Scoped directory discovery
  requires an enabled enclosing owner. Existing source attribution is retained.
- Explicitly referenced child declarations may have names unrelated to the
  owner's spelling. Validate their actual schema, owner and permitted path.
  A public mod title such as Ritualist may differ from internal ModID Conjurer.
- Retain bounded discovery/loading, cycle detection, duplicate/conflict checks
  and transactional root/shard validation. No global per-frame asset scan.

## Loose declarations (existing lane, retained)

- Use the existing module-relative `registry/` folder for supported `.json`/`.jsonc`
  declaration documents, using the managed module loader and its existing
  file/path/size limits. A file is a declaration only after schema validation.
- Derive the permitted owner from the enabled module containing the file.
  No new loose JSON owner field or parallel loader was added.
- Feed validated loose declarations into the same transaction and enrollment
  checks as cooked declarations. Existing JSONC fallback/override precedence
  remains unchanged and conflicts remain explicit.

## Discovery limits and verification

Both deterministic root names are tried directly. Other root names, including
aliases whose suffix differs from the owner, require Asset Registry metadata.
The new scoped metadata pass has a 4,096-candidate bound; it adds no PAK scan.

Focused shard tests cover legacy and aliased roots, independently named children,
mixed-case paths, cross-owner references, escaped paths, malformed declarations,
duplicates, cycles and existing transaction limits. Loader contracts and the
mixed-case registry-only module fixture also pass. A live gameplay comparison
remains necessary; these tests do not establish the behavior of every cooked mod.

## Native discovery review — October 28, 2026

The current loader already reads Unreal's Asset Registry metadata before it
loads declarations. It also tries the two conventional root paths directly,
so an omitted metadata entry does not automatically hide a conventional mod.
A generic asset name is a discovery hint: its actual owner, string property,
schema and complete shard group still have to validate.

Three steps need to agree: Unreal mounts the package, its asset catalogue makes
the asset discoverable, and the game's applicable subsystem registers the live
object for persistence/network use. Completing the first two does not establish
the third. Existing gameplay registration therefore continues through verified
subsystem contracts; cooked PAKs retain their own behavior and JSONC remains an
authoring fallback/override.

The next useful refinement is readiness-driven, scoped discovery before registry
sealing. Epic documents asset-added and files-loaded notifications and warns that
listing all assets can be slow in its
[Unreal 5.6 Asset Registry guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-registry-in-unreal-engine?application_version=5.6).
Those examples include editor-only behavior: their exact shipping-game delegate
and metadata contracts must be verified before using them through UE4SS.

Keep this as a follow-up investigation, rather than adding a repeating global
rescan or assuming a mounted PAK is registered. Compare conventional roots,
generic aliases, absent metadata and late metadata on both clients and the server.
Only proven per-owner changes should trigger bounded startup work; gameplay
transitions must not rebuild or reorder sealed registries.
