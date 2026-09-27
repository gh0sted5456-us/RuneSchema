# Save cleanup and ownership

RuneSchema removes persistent content only when it has recorded ownership for
that content and the owning mod or declaration is no longer active.

Unknown, vanilla, and third-party identities are left alone.

## Ownership record

`OwnedContentLedger.json` is a small list of persistent identities RuneSchema
loaded successfully on the previous run.

A record contains the content kind, owning mod, persistence ID, and the minimum
identity needed to compare it later. The file does not store inventory
amounts, character progress, or copies of removed mod content.

Loaders record identities they create. Cooked persistent content can use
`$declaration` when ownership cannot be inferred from a loader record.

## Cleanup flow

1. RuneSchema loads the enabled mods.
2. Successful persistent definitions form the current ownership set.
3. The current set is compared with the previous ownership record.
4. A previous identity becomes retired when its owner is removed, disabled, or
   no longer declares that identity.
5. Only missing identities that the previous record confirms belong to
   RuneSchema become cleanup candidates.
6. After the game loads the character, RuneSchema removes matching items,
   recipes, quests, journal entries, and lore entries.
7. RuneSchema saves the new ownership record only after cleanup is verified.

A missing registry lookup by itself is not proof that content is safe to
remove.

## Items and equipped gear

When a player removes a mod while wearing or carrying a RuneSchema-owned item,
the previous ownership record provides the exact persistence identity.

RuneSchema removes the owned inventory entry and related equipped/loadout
reference, then verifies that the retired identity is gone.

Removed content is not archived. Reinstalling the mod later does not restore
the deleted item or stack.

## Steam / GOG

RuneSchema waits until all mod content and the character have loaded, then
removes exact missing identities from the active character. The game writes
the next save normally.

RuneSchema state is stored separately at:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

If cleanup cannot be verified, the previous ownership record remains in place
so RuneSchema can retry later.

## Game Pass / WinGDK

Game Pass uses Xbox Game Save under the package `SystemAppData\wgs` tree.
Those files belong to Xbox Game Save and are not edited as ordinary JSON files.

RuneSchema follows the same rule as Steam/GOG: wait for the game to load the
character, remove exact missing identities, verify the result, and let
Dragonwilds save normally.

The Game Pass ownership record lives under:

```text
%LOCALAPPDATA%\Packages\<package-family>\LocalState\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

Steam and Game Pass ownership records remain separate. On either storefront,
an incomplete load prevents cleanup and keeps the previous record for retry.

For manual recovery, use
[Manual Save Recovery](MANUAL-SAVE-RECOVERY.md). Do not edit GUID-named WGS
files or `containers.index` in place.

## Failure rules

- uncertain ownership means no deletion;
- failed cleanup keeps the previous ownership record for retry;
- retrying an already removed owned identity is safe;
- one failed mod or unsupported category does not turn unknown content into a
  cleanup target;
- storefront-specific cleanup never falls back to the other storefront's save
  path.

## Author checklist

- Use a stable `PersistenceID` for persistent content.
- Do not reuse another mod's or vanilla persistence identity.
- Use the correct RuneSchema loader or a `$declaration` for cooked persistent
  content.
- Keep the owning mod folder name stable between releases.
- Treat mod removal and reinstall as a fresh install for removed persistent
  content.
- Test removal with the item both carried and equipped.
