# 0.6.2 extended authoring backport

Base: `220a7e4417325cf78b0c43cabc10ae166e522e5e` (the frozen 0.6.2 Experimental.2 tree).

## Included

- `/assets` accepts the deterministic `$Patch`/`$Target` envelope. Patch targets cannot change `PersistenceID` or `InternalName` and nested create/clone/patch directives are rejected.
- `/assets` accepts exact-case `$Clone` and `$Create` directives. `$Clone` is restricted to `ItemData`, constructs a distinct runtime object, copies only safe reflected properties, clears inherited identity, applies authored fields, and registers the new identity with `ItemSubsystem`.
- Created items receive deterministic `/Game/RuneSchema/<mod>/Items/<internal-name>` runtime paths and remain discoverable by authoring path, object path, internal name, and persistence ID.
- Extended consumable-pack handling accepts concise and native exported field spellings and resolves cloned items through the normal reflected soft-object writer.
- New items require explicit, canonical `PersistenceID` and non-empty `InternalName`; the later temporary/session identity fallback was intentionally excluded.

## Narrow dependencies

- `AssetCreateSchema`: directive validation and deterministic runtime item paths.
- `JsonPatchDirective` and `JsonLoadOrderMerge`: strict patch parsing and deterministic recursive merge rules.
- `PersistenceId`: canonical Dragonwilds persistence-ID validation.
- `AssetConsumablePackSchema` and the header-only `SpawnSchemaFields`: extended `/assets` pack validation.
- `IdentityOverride`: included only for native item identity-map registration/read-back. It is referenced only by the asset loader in this backport; recipe and journal loaders remain unchanged.

## Deliberately excluded

- Later recipe, journal, raw-table, blueprint, building, course, quest, event, NPC, dialogue, resource, and spawn-identity rewrites.
- Temporary generated item identities and their later configuration/UI controls.
- Content-ownership preflight and later global runtime registries.

## `/players` finding

The handoff does not contain a separable player loader. `/players` is embedded in the later `DragonWildsSpawnLoader` and depends on its replacement runtime cadence, controller discovery, appearance-table resolution, attribute baselines, inventory/quest helpers, and configuration system. It cannot be copied into the frozen loader without either a substantial independent extraction or importing unrelated later runtime code. It is therefore not included in this surgical asset backport.

## Verification

- Configured against pinned UE4SS commit `0bfec09e` using Visual Studio 18/MSVC 19.51 and Windows SDK 10.0.26100.
- Built successfully with configuration `Game__Shipping__Win64`.
- Output: `RuneSchema.dll`.
- Runtime game validation is still required for native item registration, clone persistence across restart, deterministic multi-mod patch ordering, and consumable-pack soft references.
