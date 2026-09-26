# SafeSave and ownership

RuneSchema removes persistent content only when it has recorded ownership for
that content and the owning mod or declaration is no longer active.

Unknown, vanilla, and third-party identities are left alone.

## Ownership ledger

`OwnedContentLedger.json` stores the persistent identities RuneSchema loaded
successfully on the previous run.

A record contains the content kind, owning mod, persistence ID, and the minimum
identity needed to compare it later. The ledger does not store inventory
amounts, character progress, or copies of removed mod content.

Loaders record identities they create. Cooked persistent content can use
`$declaration` when ownership cannot be inferred from a loader record.

## Cleanup flow

1. RuneSchema loads the enabled mods.
2. Successful persistent definitions form the current ownership set.
3. The current set is compared with the previous ledger.
4. A previous identity becomes retired when its owner is removed, disabled, or
   no longer declares that identity.
5. Only retired, ledger-confirmed identities become cleanup candidates.
6. The new ledger is committed after the applicable cleanup is verified.

A missing registry lookup by itself is not proof that content is safe to
remove.

## Items and equipped gear

When a player removes a mod while wearing or carrying a RuneSchema-owned item,
the previous ledger provides the exact persistence identity.

RuneSchema removes the owned inventory entry and related equipped/loadout
reference, then verifies that the retired identity is gone.

Removed content is not archived. Reinstalling the mod later does not restore
the deleted item or stack.

## Steam / GOG

Character saves are loose files under:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\SaveCharacters
```

RuneSchema applies the ownership-only cleanup before deserialization, keeps a
`runeschema-before-clean` backup, writes atomically, and reads the result back
before committing the new ledger.

RuneSchema state is stored separately at:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

If parsing, backup, writing, or verification fails, the previous ledger remains
pending so cleanup can retry later.

## Game Pass / WinGDK

Game Pass uses Xbox Game Save under the package `SystemAppData\wgs` tree.
Those files are provider-managed and are not treated as loose Steam JSON.

RuneSchema removes supported retired state from the hydrated player data and
lets Dragonwilds persist the result through the active Xbox save provider.

The Game Pass ledger lives under:

```text
%LOCALAPPDATA%\Packages\<package-family>\LocalState\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

Steam and Game Pass ledgers remain separate.

For manual recovery, use
[Manual Save Recovery](MANUAL-SAVE-RECOVERY.md). Do not edit GUID-named WGS
files or `containers.index` in place.

## Failure rules

- uncertain ownership means no deletion;
- failed cleanup keeps the previous ledger for retry;
- retrying an already removed owned identity is safe;
- one failed mod or unsupported category does not turn unknown content into a
  cleanup target;
- storefront-specific cleanup never falls back to the other storefront's save
  path.

## FAQ

### FAQ-SAFESAVE-001 — Does SafeSave delete every unresolved item or recipe ID? {#faq-safesave-001}

No. An unresolved identity is not enough. Cleanup requires recorded RuneSchema
ownership.

### FAQ-SAFESAVE-002 — Is OwnedContentLedger.json a save backup? {#faq-safesave-002}

No. It stores ownership identities, not inventory amounts, character progress,
or restorable copies of mod content.

### FAQ-SAFESAVE-003 — Will reinstalling a removed mod restore items that SafeSave deleted? {#faq-safesave-003}

No. Reinstalling the mod is treated as a fresh install. Removed items or stacks
are not restored from the ledger.

### FAQ-SAFESAVE-004 — Do Steam/GOG and Game Pass use the same cleanup path? {#faq-safesave-004}

No. Steam/GOG can use backed-up loose character files. Game Pass cleanup works
through hydrated game state and the active Xbox Game Save provider.

### FAQ-SAFESAVE-005 — Should I manually edit Game Pass WGS files or containers.index? {#faq-safesave-005}

No. Use the documented recovery flow instead of editing provider-managed WGS
files in place.

### FAQ-SAFESAVE-006 — What happens if cleanup cannot be verified? {#faq-safesave-006}

RuneSchema keeps the previous ownership ledger pending so the cleanup can retry
later instead of treating an uncertain result as success.


## Author checklist

- Use a stable `PersistenceID` for persistent content.
- Do not reuse another mod's or vanilla persistence identity.
- Use the correct RuneSchema loader or a `$declaration` for cooked persistent
  content.
- Keep the owning mod folder name stable between releases.
- Treat mod removal and reinstall as a fresh install for removed persistent
  content.
- Test removal with the item both carried and equipped.
