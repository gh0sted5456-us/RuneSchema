# RuneSchema 0.7.5.20 Experimental

This branch contains the clean RuneSchema 0.7.5.20 runtime and the matching
UE4SS storefront packages.

- [RuneSchema universal runtime](release/0.7.5.20/RuneSchema-0.7.5.20-Universal.zip)
- [RuneSchema core-only runtime](release/0.7.5.20/RuneSchema-0.7.5.20-Core.zip)
- [UE4SS for Steam/GOG](release/0.7.5.20/UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](release/0.7.5.20/UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)
- [Artifact hashes and installation notes](release/0.7.5.20/README.md)

The universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime
and selects an isolated native-binding lane. Both RuneSchema archives include
`RuneSchema/enabled.txt`, `RuneSchema/dlls/mappings/Mappings.usmap`, and an
empty `RuneSchema/mods` directory.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
