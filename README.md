# RuneSchema 0.7.5.9 Experimental

This branch contains the clean, buildable RuneSchema 0.7.5.9 source and its
ready-to-install artifacts.

- [RuneSchema universal runtime](release/0.7.5.9/RuneSchema-0.7.5.9-Universal.zip)
- [UE4SS for Steam/GOG](release/0.7.5.9/UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](release/0.7.5.9/UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)
- [Artifact hashes and installation notes](release/0.7.5.9/README.md)

The RuneSchema runtime is storefront-agnostic and detects Steam/GOG or Game
Pass/WinGDK at runtime. Its archive includes `RuneSchema/enabled.txt` and does
not ship a `RuneSchema/mods` payload.

Run `Build RuneSchema.bat` or `build\build.bat -Clean` to rebuild the package.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
