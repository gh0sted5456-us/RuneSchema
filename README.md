# RuneSchema 0.7.5.22 Experimental

This branch contains the clean RuneSchema 0.7.5.22 runtime and the matching
UE4SS storefront packages.

- [RuneSchema universal runtime](release/0.7.5.22/RuneSchema-0.7.5.22-Universal.zip)
- [RuneSchema core-only runtime](release/0.7.5.22/RuneSchema-0.7.5.22-Core.zip)
- [UE4SS for Steam/GOG](release/0.7.5.22/UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](release/0.7.5.22/UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)
- [Artifact hashes and installation notes](release/0.7.5.22/README.md)

The universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime
and selects an isolated native-binding lane. Both RuneSchema archives include
`RuneSchema/enabled.txt`, `RuneSchema/dlls/mappings/Mappings.usmap`, and an
empty `RuneSchema/mods` directory.

Version 0.7.5.22 extends absolute, idempotent scale reconciliation to every
RuneSchema-managed NPC, resource, actor, AI spawn, static assembly, building,
and player scale rule. It also adds write-once, appearance-only player
fallbacks under LocalAppData and declared ownership for removable custom
appearance rows.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
