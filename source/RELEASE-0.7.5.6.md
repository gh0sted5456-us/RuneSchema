# RuneSchema 0.7.5.6

## Building clones

- `$Clone` now inherits the source `BuildingPieceData` catalogue location when
  `AddTo` is omitted. The source remains present and the clone is appended as a
  separate entry.
- `AddTo` accepts one placement or an array of placements.
- Cooked `BuildableActor` overrides are fail-closed: they require `$Clone`, a
  concrete `BP_BaseBuilding_BaseActor` descendant, a valid class default
  object, and a native building-data binding.
- A custom actor override defaults to `ManagedActor` representation so the
  source's cooked lightweight mesh is not rendered instead on remote clients.
- `Requirements` completely replaces the source cost after every item and
  positive amount validates.
- Runtime identity/index fields cannot be overwritten through `Properties`.
- See `BUILDING-CLONING-FMODEL-AUDIT.md` and the `BuildingClone` example.

## Owned save cleanup

- Successful RuneSchema item clones are recorded in
  `settings/OwnedContentLedger.json`. The ledger is historical ownership
  evidence; it is not a backup and never restores removed items.
- A missing folder and a folder disabled in `runeschema.txt` are both absent
  owners.
- First-run migration can recover exact item ownership from the `assets`
  definitions of disabled-but-present mods without activating them. If a mod
  folder was already deleted, RuneSchema can also import only confirmed,
  registered item-clone identities from a valid prior
  `asset-clones-current.json` or `asset-clones-previous.json` diagnostic
  manifest. These sources seed the ownership ledger; they never restore an
  item or execute a disabled definition.
- Before native character deserialization, RuneSchema removes only exact
  absent-owner identities from inventory, personal inventory, loadout, and
  item/recipe progress. It also clears loadout records that point at a removed
  inventory slot. A sibling backup is created before the character JSON is
  atomically replaced and read back for verification.
- Temporary registry tombstones and the verified post-load native scrub remain
  as a second line of defense for records that survive the file preflight.
- The next normal game save persists the cleaned inventory. Reinstalling the
  mod later does not restore removed stacks.
- Quest/dialogue and journal cleanup retain their existing embedded ownership
  metadata. Building registry retirement retains historical building indices.
- Vanilla records, third-party content not created through RuneSchema, and
  merely unknown registry IDs are not removed by the automatic pass.
- Cleanup failure is non-destructive and tagged `[SAVE-CLEANER][DEGRADED]`;
  the record is retained rather than guessed away.

This is intentionally not the previous broad unresolved-ID scan. It does no
global object walk. Character JSON files are scanned only when the historical
ledger contains content for a confirmed absent or explicitly disabled owner;
ordinary startup does no save-file scan.
If a mod was deleted before 0.7.5.6 and neither a historical ledger nor a prior
clone manifest exists, RuneSchema deliberately refuses to guess which unknown
save ID belonged to that mod.
