# Game Pass save system

RuneScape: Dragonwilds uses Xbox Game Save (WGS) for the WinGDK build. The
active package family is discovered at runtime; the current retail family is
`JagexLimited.Dominion_srxstwq7wczqa`. Its provider-managed root is:

```text
%LOCALAPPDATA%\Packages\JagexLimited.Dominion_srxstwq7wczqa\SystemAppData\wgs
```

Each signed-in Xbox profile is a directory beneath that root. The profile has
a version-14 `containers.index`; its entries point to GUID-named container
directories and `container.N` blob tables. The files with GUID names are not
independent Steam saves and must not be selected or rewritten as though they
were ordinary JSON files.

## Dragonwilds payloads

The save formats were checked against the current WinGDK profile and the
public RSDW Save Converter implementation:

- Character entries end in `Qjson`. Their `Data` blob is plain UTF-8 JSON and
  has the same `GameProgress` document consumed by the Steam character-save
  cleaner.
- World entries end in `Qxav`. Their `Data` blob begins with three
  little-endian 32-bit values: header size `12`, block size `65536`, and the
  uncompressed length. The remaining bytes are a zlib-wrapped `SAVE` payload.
- `Qbak` entries are the game's backup slots. RuneSchema does not replace
  those provider backups.

References:

- <https://github.com/RSDWArchive/RSDWSaveConverter>
- <https://github.com/LukeFZ/XblContainerReader>
- <https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/ISaveGameSystem>

## RuneSchema cleanup lanes

Both storefronts use `SaveCleanup::PlanOwned` semantics and the same compact
ownership ledger: only persistence IDs previously declared by RuneSchema and
now absent or disabled are eligible. Vanilla and unowned third-party records
are preserved.

Steam/GOG runs the plan over loose character JSON before deserialization,
keeps a `runeschema-before-clean` backup, writes atomically, reads the result
back, and only then commits the new ownership snapshot.

Game Pass cannot safely use that file path. RuneSchema registers temporary
tombstones before character hydration, removes retired item and recipe state
from the hydrated controller, reads the live collections back, and lets the
game persist the result through its active WGS provider. The old ownership
snapshot is retained until every pending category has a verified adapter.
Unsupported categories do not prevent supported item and recipe cleanup; they
remain pending and retry without being forgotten.

This split is intentional. Editing `containers.index` while Dragonwilds or
the Xbox provider is active can race provider state and cloud synchronization.
Offline conversion tools therefore require the game to be closed, back up the
complete WGS profile, create new payload/table files, replace the index
transactionally, and verify the installed payload. RuneSchema's in-process
runtime does not perform that offline operation.

## RuneSchema-owned state

The Game Pass ownership ledger is separate from WGS and lives at:

```text
%LOCALAPPDATA%\Packages\<package-family>\LocalState\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

Steam/GOG uses:

```text
%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\safesave\OwnedContentLedger.json
```

The two lanes may be seeded once, but they do not overwrite one another after
that point.

## Manual recovery

Do not edit GUID-named WGS files or `containers.index` directly. For a
backup-first extraction, appearance repair, and verified reimport procedure,
see [MANUAL-SAVE-RECOVERY.md](MANUAL-SAVE-RECOVERY.md).
