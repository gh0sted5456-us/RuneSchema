# Persistent-content cleanup

RuneSchema uses one small ownership vector, not a second save system. Player
inventory, recipes, quests, journals, appearance, and progress remain in the
game's native save.

`OwnedContentLedger.json` contains only the persistent identities that
successfully loaded on the preceding run, plus their loader kind and owner.
It contains no player data, quantities, progress, or restorable mod content.
The file is overwritten after a successful comparison; it is not a history.

## Lifecycle

1. Load every enabled mod and register its content.
2. Add each successful persistent definition to the current identity vector.
3. If any loader section fails, mark the vector incomplete and do no pruning.
4. After native item, recipe, and quest registration finishes, compare the
   current vector with the preceding successful vector.
5. Treat only exact RuneSchema-owned IDs missing from the current vector as
   retired.
6. After the game hydrates the character, remove those exact retired IDs from
   live native state and verify their absence.
7. Commit the current vector only after all applicable cleanup is ready and
   verified. If a component is not ready, retain the preceding vector and
   retry at the next native load notification.

The important boundary is registration before pruning. A temporarily missing
registry, failed mod section, or storefront-specific hook can never authorize
deletion. Quest identities enter the current vector during loader processing,
before quest subsystem preparation, so their registration cannot collide with
a false retired placeholder.

## Storefront lanes

Steam/GOG and Game Pass use their own runtime detection and state locations,
but the cleanup rule is identical: operate on the character's live native
state after registration. Automatic cleanup does not edit Steam JSON files or
the Xbox WGS container database directly.

The ownership vector is stored below RuneSchema's storefront-specific state
directory in:

```text
settings/safesave/OwnedContentLedger.json
```

## What is removed

- Inventory/equipment items: exact retired `PersistenceID` only.
- Recipe unlocks: exact retired recipe identity only.
- Quest progress: exact retired quest identity with the matching RuneSchema
  ownership marker only.
- Journal and lore: exact retired RuneSchema identity removed from the
  hydrated `JournalComponent` after its native persistence callback.
- Categories without a verified live adapter are retained. Their presence
  prevents final vector commit rather than triggering a speculative edit.

RuneSchema never automatically removes every ID missing from a partially
available game registry. Reinstalling a removed mod is a fresh installation;
deleted content is not restored.

## Cooked content

Loader-created objects are recorded automatically. Cooked PAK content can use
`$declaration` to state its path and `PersistenceID`. A declaration establishes
RuneSchema ownership for later exact cleanup; it does not modify the asset.

## Author requirements

- Give persistent content a stable, unique `PersistenceID`.
- Never reuse a vanilla or another mod's identity.
- Use a RuneSchema loader or `$declaration` for cooked persistent content.
- Test removal with the content present in a character save.
