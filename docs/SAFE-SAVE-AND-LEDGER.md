# SafeSave and ownership ledger

RuneSchema removes only content it can prove belongs to RuneSchema and to a mod
or declaration that is no longer active. It does not treat every unresolved
game identity as disposable.

## What the ledger stores

`OwnedContentLedger.json` is a compact snapshot of persistent identities that
successfully loaded during the previous run. A record includes its kind,
owning mod, persistence ID, and the minimal internal identity required for a
safe comparison. It does not contain inventory quantities, player progress,
save backups, or restorable copies of mod content.

Loaders record identities they create or explicitly receive through
`$declaration`. Declarations are appropriate for cooked PAK content when the
JSON loader cannot infer ownership safely. They mark ownership only; they do
not change the cooked object.

## Startup comparison

1. RuneSchema discovers enabled mods and processes their loader files.
2. Successful persistent definitions form the current snapshot.
3. The previous snapshot is compared with the current one.
4. A previous identity becomes retired when its owner is removed or disabled,
   or when that specific declaration disappears.
5. Only those retired, ledger-confirmed identities become cleanup candidates.
6. The new snapshot is committed only after the applicable cleanup transaction
   is verified.

An active definition is retained. Vanilla content and unknown third-party
identities are retained. RuneSchema does not guess from a name prefix or from
the mere fact that a native registry lookup failed.

## Equipped armor and inventory items

If a player saved while wearing RuneSchema armor and then removes or disables
the owning mod, the previous ledger supplies the exact `PersistenceID`.
RuneSchema makes that retired identity temporarily resolvable during load,
removes its inventory/personal-inventory record, removes the related equipped
or loadout reference, and verifies that the item is absent. This prevents the
save from retaining an equipment reference whose DataAsset no longer exists.

The item is deleted, not archived. Reinstalling the mod later is a fresh
installation and does not restore the removed item.

## Steam/GOG transaction

Steam/GOG character saves are ordinary JSON files under:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\SaveCharacters
```

Before deserialization, RuneSchema parses each bounded character file, applies
the ownership-only plan, verifies the resulting JSON, preserves the original
as a `runeschema-before-clean` backup, writes atomically, and reads the result
back. A failed parse, verification, backup, or write leaves the ledger pending
for a later retry.

RuneSchema state is stored separately at:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

## Game Pass/WinGDK transaction

The current Dragonwilds package family stores Xbox Game Save data beneath:

```text
%LOCALAPPDATA%\Packages\JagexLimited.Dominion_srxstwq7wczqa\SystemAppData\wgs
```

Character `Qjson` payloads contain plain UTF-8 character JSON, but they are
addressed through a WGS `containers.index`, GUID container directories, and
blob tables. RuneSchema does not rewrite that database while the game and Xbox
provider are active.

Instead, RuneSchema registers retired item identities before the character is
hydrated, removes supported retired state from the live player collections,
reads those collections back, and lets the game's Xbox provider persist the
clean result. The Game Pass ledger lives at:

```text
%LOCALAPPDATA%\Packages\<package-family>\LocalState\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

If an unsupported category is also pending, item and recipe cleanup still
continues. The previous ledger remains uncommitted for the unsupported category
so a later launch can retry it. That pending category cannot redirect Game Pass
into Steam's file path.

## Failure and recovery rules

- Missing player components, failed native calls, failed read-back, malformed
  save structure, or unavailable provider callbacks retain the previous
  snapshot for retry.
- The cleaner never converts an uncertain result into success.
- Cleanup is idempotent: retrying an already absent owned identity is safe.
- An unrelated mod failure does not disable cleanup for a verified item or
  recipe category.
- Game Pass and Steam ledgers remain separate after the one-time migration or
  seed. One storefront cannot overwrite the other's current snapshot.

## Author checklist

- Give every persistent addition a stable `PersistenceID`.
- Do not reuse a vanilla or another mod's persistence identity.
- Ship the definition through the appropriate RuneSchema loader, or add a
  `$declaration` for cooked persistent content.
- Keep the owning mod folder name stable between releases.
- Treat removal and reinstallation as destructive reset behavior.
- Test a character with the item in inventory and equipped, then disable the
  mod and confirm the save loads and the owned item disappears.
