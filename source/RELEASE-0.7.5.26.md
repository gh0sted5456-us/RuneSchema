# RuneSchema 0.7.5.26

This release separates loader activation from journal/lore and recipe save
persistence.

## Persistence controls

Two settings are available in `settings/settings.jsonc` and under RuneSchema
Settings > General > Runtime:

```jsonc
"persistence": {
  "characterCustomization": false,
  "journal": false,
  "recipes": false
}
```

Both settings default to `false`.

- `recipes: false` keeps RuneSchema recipes available for the current session
  through Dominion's `RecipesUnlockedThatShouldNotPersist` set. RuneSchema does
  not add them to the permanent `RecipesUnlocked` set.
- `characterCustomization: false` keeps character option/table loaders active
  while preventing automatic `/players` appearance rules from rewriting the
  native `CustomizationSaveData` structure. Vanilla editor confirmations are
  still owned by the game itself.
- `journal: false` keeps journal and lore asset creation, registration, and
  category placement active, but skips `UnlockJournalEntry`. Dominion does not
  expose a verified transient journal-unlock collection, and that native call
  would otherwise dirty the player save.
- Enabling either setting restores the corresponding save-backed unlock path.
- The `/journal`, `/lore`, and `/recipes` loader switches remain independent;
  disabling persistence does not disable those loaders.

This behavior is identical at the configuration layer on Steam/GOG and Game
Pass/WinGDK. It does not route either storefront through the other storefront's
native save adapter.

## Game Pass character preview

- The live WinGDK wearable sequence was reverified at RVA `0x6CF4E77`, with
  the configured hook point at RVA `0x6CF4EB4`.
- The character-menu fallback no longer stops after its first successful
  preview lookup. While an equipment item has a RuneSchema visual effect, the
  preview is refreshed at a bounded menu-only cadence, covering WinGDK builds
  that do not emit all of Steam's secondary appearance callbacks.
- The fallback remains usable if a native appearance hook is unavailable and
  is removed as soon as no preview item effects remain.

## Game Pass SafeClean transaction

- Xbox Game Save (`SystemAppData/wgs`) is never opened or rewritten as a loose
  Steam character JSON file. Cleanup continues through the hydrated live
  inventory/progress objects and the game's active WinGDK save provider.
- The previous Game Pass ownership snapshot is no longer overwritten when
  cleanup is merely scheduled. It remains intact until every retired item
  count and recipe set has been read back as clean after the provider load.
- A crash, missed load event, unavailable component, or failed verification
  leaves the previous snapshot in place so the next launch retries.
- Retired save categories without a verified WinGDK live adapter fail closed:
  no partial provider cleanup is attempted and the ledger is retained. Steam's
  independently backed-up JSON route is unchanged.

## Verification

- Configuration round-trip coverage verifies both defaults and explicit opt-in.
- A release-gate contract verifies persistence controls remain separate from
  loader activation and that recipes continue using the native transient set.
- A preview contract guards the bounded storefront-neutral refresh fallback.
- SafeClean contracts guard delayed Game Pass snapshot commit, retry behavior,
  unsupported-category refusal, and strict separation from Steam JSON files.
- The previous storefront, equipment, journal, mesh, and storage contracts
  remain part of the release gate.
