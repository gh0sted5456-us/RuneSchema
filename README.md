# RuneSchema 0.7.5.25 Experimental

This branch contains the clean RuneSchema 0.7.5.25 runtime and the matching
UE4SS storefront packages.

- [RuneSchema universal runtime](release/0.7.5.25/RuneSchema-0.7.5.25-Universal.zip)
- [RuneSchema core-only runtime](release/0.7.5.25/RuneSchema-0.7.5.25-Core.zip)
- [UE4SS for Steam/GOG](release/0.7.5.25/UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](release/0.7.5.25/UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)
- [Artifact hashes and installation notes](release/0.7.5.25/README.md)

The universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime
and selects an isolated native-binding lane. Both RuneSchema archives include
`RuneSchema/enabled.txt`, `RuneSchema/dlls/mappings/Mappings.usmap`, and an
empty `RuneSchema/mods` directory.

Version 0.7.5.25 preserves the 0.7.5.24 WinGDK journal and mesh corrections,
independently verifies Steam Windstep/Shadowveil and all eight Surge/Dash
sites, and isolates Xbox WGS cleanup/state from Steam's loose save files.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
