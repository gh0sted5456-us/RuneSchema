# RuneSchema 0.7.5.26 Experimental

This branch contains the clean RuneSchema 0.7.5.26 runtime and the matching
UE4SS storefront packages.

- [RuneSchema universal runtime](release/0.7.5.26/RuneSchema-0.7.5.26-Universal.zip)
- [RuneSchema core-only runtime](release/0.7.5.26/RuneSchema-0.7.5.26-Core.zip)
- [UE4SS for Steam/GOG](release/0.7.5.26/UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](release/0.7.5.26/UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)
- [Artifact hashes and installation notes](release/0.7.5.26/README.md)

The universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime
and selects an isolated native-binding lane. Both RuneSchema archives include
`RuneSchema/enabled.txt`, `RuneSchema/dlls/mappings/Mappings.usmap`, and an
empty `RuneSchema/mods` directory.

Version 0.7.5.26 adds independent character-customization, journal/lore, and
recipe persistence controls, all disabled by default. Loaders remain enabled:
recipes use the game's transient unlock set, journal/lore registration and
placement continue without save-backed player unlocks, and character option
tables load without automatic `/players` save rewrites.
The WinGDK equipment-preview path also keeps a bounded menu-only refresh
fallback so ghost materials update even without Steam's secondary callbacks.
Game Pass SafeClean now retains its previous identity snapshot until
provider-backed item/recipe cleanup is read-back verified; failed or unsupported
cleanup remains pending for retry and never falls through to Steam JSON paths.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
