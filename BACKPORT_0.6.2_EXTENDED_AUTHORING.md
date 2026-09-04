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

## `/players`

- `/players` is integrated into the existing 0.6.2 spawn/runtime loader and its engine tick; no later spawn, quest, event, inventory, dialogue, or NPC runtime was imported.
- Rules are re-evaluated once per second, so players joining later and replacement pawns created on respawn receive their rules.
- `PlayerName`, `PlayerNames`, `PlayerGuid`, and `PlayerGuids` selectors are supported.
- `*` selects all connected players. `*1`, `*2`, and subsequent numbered wildcards select stable first-seen connection slots for the current process rather than controller-enumeration order.
- Scale and absolute `MaxHealth`/`BaseHealth` use the native actor and health APIs. Other reflected pawn fields are written through the existing 0.6.2 property helper; unsupported fields fail safely and remain pending.
- A rule is applied once to each pawn instance, preventing repeated multiplication while naturally reapplying after respawn.

## Verification

- Configured against pinned UE4SS commit `0bfec09e` using Visual Studio 18/MSVC 19.51 and Windows SDK 10.0.26100.
- Built successfully with configuration `Game__Shipping__Win64`.
- Output: `RuneSchema.dll`.
- Runtime game validation is still required for player join/respawn ordering, native item registration, clone persistence across restart, deterministic multi-mod patch ordering, and consumable-pack soft references.
