# Runtime efficiency review — 2026-09-06

## Findings and changes

ReadyGate2 remains the behavioral baseline. The rejected BPLive1 instance-registration change is excluded. Main-loader and Blueprint-loader executable logic compare equal after normalizing the intentionally changed log levels.

The asset batch previously enumerated ItemSubsystem objects twice for each new clone: creation and registration. It now performs one lazy lookup per synchronous batch and passes the pointer directly; no world-owned pointer survives the batch. Initial journal application previously reapplied every queued definition for each journal-owning mod. The first successful pass now suppresses those repeats, without moving the phase or suppressing retries after an error. Manual auto-reload remains available.

Logging forwards arguments instead of copying them at each wrapper. Notification channels use string views. Per-item recipe creation/placement, clone identity details, Blueprint details, mod loading and signature addresses are advanced verbose output. Normal output contains clone/patch totals, table summaries and errors. Blueprint summaries deliberately count registered rules, because registration alone does not establish successful live editing. Startup trace file rotation and flushing are disabled unless `enableDebugLogging` is enabled. The setting defaults to false and appears as **Advanced verbose logging** in Settings.

Removed the unused/unimplemented raw-loader `Initialize` declaration and empty destructor body. The broader keyword/caller scan did not justify removing historical building records, legacy spawn GUID matching or any of the nine native signatures: all remain active compatibility mechanisms. Attribution and short ABI/ownership comments stay in source. Ordinary comments and documentation do not contribute to DLL size.

## Primary-source comparison

- [PalSchema main loader](https://raw.githubusercontent.com/Okaetsu/PalSchema/main/src/Loader/PalMainLoader.cpp), inspected online: uses DataTable serialization, GameInstance initialization, a pak-folder hook and game-thread auto-reload dispatch. RuneSchema already uses these mechanisms. Copying its early first-table initialization back would discard the tested readiness correction. Its teardown releases hooks and callbacks; RuneSchema additionally stops its watcher before destroying consumers. No arbitrary startup delay was introduced.
- [PalSchema installation requirements](https://okaetsu.github.io/PalSchema/docs/gettingstarted) specify its matching UE4SS build. That supports retaining Dragonwilds' validated ABI, not replacing it with Palworld's binary.
- [UE4SS BPModLoaderMod](https://github.com/UE4SS-RE/RE-UE4SS/blob/main/assets/Mods/BPModLoaderMod/Scripts/main.lua) uses world lifecycle callbacks and game-thread execution. The relevant lesson is controlled lifecycle work rather than recurring scans. RuneSchema's existing world callbacks and disabled-by-default tools remain unchanged.
- [BPML GenericFunctions](https://raw.githubusercontent.com/UE4SS-RE/RE-UE4SS/main/assets/Mods/BPML_GenericFunctions/Scripts/main.lua) validates reflected arguments and constructs persistent objects with a GameInstance owner. It does not establish a general guarantee of safe arbitrary DLL unloading.
- [UE4SS C++ lifecycle documentation](https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html) places Unreal API use after Unreal initialization. The ReadyGate2 core-readiness boundary remains intact; early Dragonwilds compatibility bootstrap and pak interception are retained.

## Startup pause and silent crashes

The preserved successful-launch log shows UE4SS scanning/reflection work during the opening seconds, after RuneSchema's loaded marker. Its own initial signature scan took about 459 ms; subsequent discovery and reflection continued for several seconds. Earlier RuneSchema startup audits measured its custom signature scan around 1.25–1.30 seconds. These measurements explain why removing RuneSchema console output alone cannot promise to eliminate the visible/audio pause. No unsafe signature-address disk cache or change to UE4SS early scanning was made.

Windows Application events at 14:10 and 14:13 report Dragonwilds `0xc0000374` heap corruption. The retained WER reports contain module lists and exception metadata, but no allocation stack or usable dump identifying the writer. UE4SS, RuneSchema, LootMenu and MiniMap were loaded; presence is not attribution. `ntdll` is where corruption was detected, not proof of the source or of a graphics-driver fault. Reports/logs are preserved under `outputs/runtime-consolidation`.

The UE4SS `InvalidateCacheIfDLLDiffers` setting is passed as `bInvalidateCacheIfSelfChanged` in the local UE4SS source. That is not evidence that swapping RuneSchema intentionally requires a failed launch. The local signature scanner serializes match callbacks under its scanner mutex, so an additional RuneSchema mutex around signature insertion would be redundant. Duplicate Gravetide pak/IoStore copies were byte-identical and reduced to one set; this removes unnecessary duplicate mount candidates but does not establish the heap-crash cause.

## Mod migration and validation

25 mod folders and 219 JSON/JSONC files become 10 folders and 64 JSON files. ArmorCollection contains individual set files, including four Ghostly Warden clones. All 132 existing PersistenceID/InternalName pairs remain unchanged; 331 checked generated item references resolve to the collection's clone definitions. Item/table/recipe/journal/equipment content compares equal after namespace migration, excluding the added Warden set. Source stat handles for Warden were verified against the four FModel item exports.

Four persistent-spawn owner folders retain their original names because owner names contribute to saved actor GUIDs. Renaming them could duplicate saved actors. The remaining content is grouped into ArmorCollection, ResourcesAndLoot, Weapons, PlayerProfiles, WorldAppearance and ZZ_CollectionPatches. Two player-wide ghost patches are removed; equipment and world appearance remain available. External mods outside the reviewed tree may require updates to old runtime item paths. Matching client/server mod content remains necessary.

Production-extracted tests cover asset batch lookup/counts/unresolved behavior and journal dispatch/retries. The actual startup trace implementation was tested with logging off/on/off. ReadyGate2 queue/replay tests were rerun. Shipping optimization, UE4SS imports, archives and staged JSON/reference checks are recorded beside the packages. Gameplay, multiplayer, first-launch stability and the audible pause still require real-game validation; no crash-fix claim is made.
