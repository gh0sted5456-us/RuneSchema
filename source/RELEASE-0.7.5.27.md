# RuneSchema 0.7.5.27

This release hardens Game Pass SafeClean after validating the current
Dragonwilds Xbox Game Save profile and payload formats.

## Xbox save discovery

- RuneSchema now resolves the active package family's provider root at
  `%LOCALAPPDATA%\Packages\<family>\SystemAppData\wgs` and includes it in
  verbose SafeClean diagnostics.
- Current Dragonwilds WinGDK character slots are `Qjson` entries whose `Data`
  blobs are plain UTF-8 character JSON. World `Qxav` entries use a 12-byte
  little-endian header followed by zlib data.
- RuneSchema does not rewrite `containers.index` or GUID blob files while the
  game is active. Hydrated state continues through the game's Xbox save
  provider, preventing a race with provider locks and cloud synchronization.
- `GAMEPASS-SAVE-SYSTEM.md` documents the container layout, both ledger roots,
  the storefront split, and the transaction boundary.

## SafeClean isolation

- A pending unsupported Game Pass save category no longer blocks verified
  item and recipe cleanup. Supported state is removed and read back normally.
- The old ownership snapshot is still retained while any unsupported category
  remains. This preserves retry information without leaving retired items in
  the hydrated inventory merely because a separate category lacks an adapter.
- Steam/GOG keeps its independent backed-up, atomic JSON path unchanged.
- Cleanup remains owned-only. Vanilla and unowned third-party IDs are not
  inferred to be removable.

## Verification

- The release contracts assert that unsupported provider categories cannot
  short-circuit item/recipe cleanup.
- Storage contracts assert package-family WGS discovery without treating WGS
  as a loose save directory.
- The current local WGS snapshot was parsed read-only: the v14 index, v4 blob
  tables, plain character JSON, and wrapped world payload all matched the
  documented format. No live Xbox save file was modified during validation.
