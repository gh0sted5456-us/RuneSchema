# RuneSchema 0.7.5.25

This release preserves the verified 0.7.5.24 WinGDK journal and mesh fixes and
tightens equipment and save-storage separation between storefront lanes.

## Equipment parity

- The current Steam executable was fingerprinted independently as timestamp
  `0x6BAC8379`, image size `0x0DDEC000`, SHA-256
  `DFDF1114FD812D646DC60E97E44E5DBA2AAF96EE924C6F6180FA621D462D76F2`.
- All eight Steam Surge/Dash hook and resume sequences match the installed
  executable byte-for-byte.
- All five Steam Shadowveil action sequences match byte-for-byte.
- Surge/Dash native hooks now explicitly refuse every non-Steam native lane.
- Windstep remains a cooked `GrantedEffects` assignment resolved through
  Unreal reflection, so it is shared by Steam/GOG and Game Pass.

## Save storage parity

- Steam/GOG continues to back up and clean loose character JSON files under
  `%LOCALAPPDATA%\RSDragonwilds\Saved`.
- Game Pass is recognized as Xbox Game Save (`SystemAppData\wgs`) storage.
  RuneSchema never rewrites the WGS provider database as loose files.
- Game Pass owned-content cleanup runs after the game loads its provider
  payload, allowing the game to write the cleaned state through its active
  Xbox save-provider lock.
- RuneSchema's Game Pass ownership ledger and appearance state are isolated in
  the package `LocalState\RSDragonwilds\Saved\RuneSchema` directory. A verified
  one-time copy seeds it from the older shared ledger without deleting or
  changing the Steam ledger.

## Verification

- The universal DLL compiles successfully.
- All 34 release-gate contracts pass, including dedicated equipment-lane and
  storefront-aware storage tests.
