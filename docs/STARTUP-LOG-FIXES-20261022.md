# Startup diagnostics — 2026-10-22

The pre-EXE performance changes remain in place. These corrections address the subsequent Steam startup scan without hiding failures.

- Read legacy version 3 PAK directory indexes, with bounded ranges and SHA-1 verification. VendorBanners' actual container is version 3 and contains 15 package paths; it is not a malformed modern PAK.
- Treat the native `Content/Paks/global.utoc` global-container header as metadata-only when its identity and table layout match. It contributes no asset ownership claims. Missing directory indexes elsewhere remain unavailable; remote snapshots cannot relabel a mod container as native global metadata.
- Native building classes need not expose the ore/tree `bSkipSpudStore` field. Managed BuildingProps may instead use the validated SPUD `ShouldSkip` return contract. Only exact serial-valid actor/function pairs created and marked by RuneSchema receive that override. Native/player buildings are untouched. Registration and actor creation remain fail-closed if the contract or round-trip cannot be verified.
- Reject unrelated functions by cached FName before the protected-building global callback scans any actor ownership.

Tests cover legacy PAK compression entry layouts, truncation, path traversal and SHA mismatch; global metadata identity and remote ownership boundaries; and managed-building hook scope/return-contract source regressions. Native world save/reload still requires a separate gameplay check.

The UE4SS Lua `FObjectPtrProperty` size advisory is a host reflection-table message, not an RPC payload limit. No guessed property size or UE4SS host replacement is included. NVIDIA's NGX malformed mapping/PlatformError comes from its shared model updater; no driver-owned configuration or graphics setting is modified. Recheck both after restarting Steam.

## First installed Steam check

Helpy starts and the frontend loads. Cache discovery now reports 48 indexed containers, one native metadata-only container, zero unavailable, and 210633 package associations. The furnace still fails closed: its actual runtime class does not satisfy the fallback's validated save-exclusion contract. Do not present that adapter as a completed furnace fix. No character/world save/reload was attempted. UE4SS's size advisory and NVIDIA's shared mapping message also remain.

## Icon manifest and mapping reuse follow-up

The live icon-source manifest is over 16 KB. Helpy incorrectly used a fixed 4 KB response buffer for `IconCacheSources`, so it never obtained a live verified manifest and new captures stayed deferred in the menu and world. This read-only operation now uses the existing bounded response-size probe. A production transport test reproduces the old failure with a 20 KB manifest and checks that source verification clears the provisional state. Capture concurrency, visible-demand polling and source validation are unchanged.

Automatic mapping compatibility v3 keys on the game executable/version and native game/feature containers. It no longer includes RuneSchema/UE4SS DLLs, managed mod definitions or load order. The transition invalidates the old key once; subsequent launches reuse a validated mapping until game inputs change or the cache is missing/invalid. Platform separation and mapping parser validation remain. This is native build metadata, not permission to trust stale mod objects; live reflection guards remain authoritative for every native operation.

The icon/mapping follow-up was compressed and installed on Steam, Game Pass and the specified server profile. All 222 core and 83 Helpy tests pass; the transport regression fails before the fix and passes after. UPX round-trip and 285 UE4SS imports per host pass. Six DLL files changed; 87 protected configuration/host/proxy files were verified unchanged, with rollback backups.

Steam restarted at 20:43:11 local time to `L_FrontEnd`. Helpy is ready, a native icon reply is logged, 43 thumbnail files were newly written/read, and the user confirmed icons are loading. The first v3 mapping was validated and cached; three retired generated cache files were removed, custom files retained. A second live launch has not yet been performed to demonstrate reuse. Discovery currently reports 50 indexed containers, one metadata-only, zero unavailable. The furnace exclusion error remains; no world/save test was performed. The installed UE4SS commit's Lua initializer catches `StaticValueSize()` failure and publishes zero before loading the first mod; changing a RuneSchema script or transport limit cannot remove that host initialization warning. Do not patch host memory with a guessed size.

## Object-pointer Lua metadata follow-up — 2026-10-23

RuneSchema now prepares the UE5 host's exported `FObjectPtrProperty` value-size metadata before Lua mods start. It fills only the missing sentinels `0`/`-1`, using the host's discovered `FObjectProperty` value size only when it equals the native pointer width. Existing sizes, field-class pointers, property offsets and hosts without the required exports remain untouched. The destination must be committed, writable data in the loaded UE4SS module; no binary patch, protection change, polling or replacement UE4SS DLL is used. A successful repair produces one normal `[UE4SS][LUA]` log entry.

The policy tests cover both missing sentinels, preservation of existing/unknown values, rejection of unverified reference sizes, idempotence and initialization order. This targets the Lua metadata advisory, not a demonstrated Helpy crash. BPModLoaderMod's separate `Game == nil` script failure is outside this change.

Installed on Steam, Game Pass and the specified server profile with rollback backups; three RuneSchema DLLs changed and 93 protected files matched their pre-install hashes. Fresh Steam and Game Pass launches both logged the metadata repair and exposed `FObjectPtrProperty` as `0x8` without the former warning. Both Helpy frontends opened and both games exited normally. RuneSchema logged no errors/warnings in either session; Steam UE4SS was clean, while Game Pass retained the separate BPModLoaderMod failure. Steam's game log also retained the existing read-only `BuildingSettings.ini` shutdown cleanup warning. All 85 compared save files were unchanged; no new crash report appeared. The server was not launched and this follow-up did not load a world.
