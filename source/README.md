# RuneSchema 0.7.5.9

RuneSchema is a UE4SS runtime for self-contained RuneScape: Dragonwilds
content mods. It loads validated JSON/JSONC definitions, connects them to
mounted cooked assets, and provides multiplayer presentation and authority
bridges without replacing vanilla game systems.

Run `..\build\build.bat -Clean` to build the release:

- One universal RuneSchema DLL detects Steam/GOG or Game Pass/WinGDK at runtime.
- Steam/GOG uses UE4SS's native object/reflection APIs and validated native signatures.
- Game Pass uses its matching UE4SS runtime and `UE4SS_Signatures` overrides where supplied.

Helpy is built once as a storefront-neutral RuneSchema API client. The output
is `dist\RuneSchema-0.7.5.9-Universal.zip`; the two verified UE4SS runtime ZIPs
are copied beside it. `-Clean` recreates build and distribution directories.

Based on the original RuneSchema 0.6.0 from Snorkles. This version is
maintained by members of the RSDW Modding Community. PalSchema foundation by
Okaetsu.
