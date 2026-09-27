# Built-in save scrubbing and ownership ledger

Save scrubbing is part of RuneSchema startup. It is not an optional feature or
a separate restore system. RuneSchema first loads active definitions and builds
the same native item, recipe and quest registries used by the game. Only after
those maps are complete does it remove unresolved persistent records. A partial
registry can never authorize deletion.

## What the ledger stores

`OwnedContentLedger.json` is a compact snapshot of persistent identities that
successfully loaded during the previous run. A record includes its kind,
owning mod, persistence ID, and the minimal internal identity required for a
safe comparison. It does not contain inventory quantities, player progress,
save backups, or restorable copies of mod content.

The player save remains the only home for inventory, equipment, recipe
unlocks, quest state, journal/lore unlocks and appearance. The ledger is not a
second save system: it is only the smallest ownership snapshot needed to prove
which exact missing IDs RuneSchema may remove on the next startup.

Loaders record identities they create or explicitly receive through
`$declaration`. Declarations are appropriate for cooked PAK content when the
JSON loader cannot infer ownership safely. They mark ownership only; they do
not change the cooked object.

## Startup sequence

1. RuneSchema discovers enabled mods and processes their loader files.
2. Successful persistent definitions form the current snapshot.
3. The previous snapshot is compared with the current one.
4. A previous identity becomes retired when its owner is removed or disabled,
   or when that specific definition or declaration disappears.
5. Retired, ledger-confirmed RuneSchema identities become exact cleanup
   candidates in both storefront lanes.
6. After active custom data is inserted into the native maps, the first
   complete item/recipe registry publication triggers one bounded menu-phase
   pass. Inventory, loadout, item progress, recipe progress and complete quest
   registries are scrubbed of unresolved identities before world entry.
7. The new ownership snapshot is committed only after its applicable cleanup
   transaction is verified.

Cleanup is keyed by the exact retired `PersistenceID`, not merely by the mod
folder. Removing one recipe, quest, journal entry, lore entry, building or item
from an otherwise active mod cannot retire its siblings. The saved quest or
journal ownership marker must also agree with the historical owner before that
record is removed.

Items, recipes, quests and buildings use the game's canonical 22-character
identity form. Journal and lore use their native readable entry IDs, such as
`RS_Journal_Dawnveil_Armor`; those IDs are intentionally preserved in both the
player save and the minimal ownership snapshot.

An active definition is retained because it is present in the final native
registry. RuneSchema does not guess from a name prefix. Steam/GOG can also
remove any nonempty inventory, equipment, item-progress, recipe-progress or
quest identity that is absent from the complete native registry. Journal and
lore remain exact ownership-ledger operations because the game does not expose
an equivalent complete persistence map for those categories.

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

Before deserialization, RuneSchema first applies exact retired ownership and
then performs one registry-complete pass in the menu. It parses each bounded
character file, verifies the resulting JSON, preserves the original under the
RuneSchema state directory, writes atomically, and reads the result back. A
failed parse, verification, backup, or write leaves the file unchanged or its
original recoverable and reports a degraded result.

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
