# RuneSchema 0.7.5.14 Experimental

Ready-to-install artifacts from the clean 0.7.5.14 universal build:

- [RuneSchema universal runtime](RuneSchema-0.7.5.14-Universal.zip)
- [RuneSchema core-only runtime](RuneSchema-0.7.5.14-Core.zip)
- [UE4SS for Steam/GOG](UE4SS-3.0.1-f6d5f942-Steam-GOG.zip)
- [UE4SS for Game Pass/WinGDK](UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip)

Install the UE4SS archive matching the storefront, then install the universal
RuneSchema archive. Use the core-only archive when Helpy and Networking are not
wanted. Both RuneSchema archives contain `RuneSchema/enabled.txt` and an empty
`RuneSchema/mods` folder.

This revision moves the optional map to `RuneSchema/dlls/mappings`, moves the
owned-content snapshot to `RuneSchema/settings/safesave`, and improves Helpy's
consistent sizing, text scale, and bounded visible-first icon cache.

## SHA-256

```text
7023E76A40D8CCA6392946652DEF694834A827273ECAC657EA4C7A8F5D4558A8  RuneSchema-0.7.5.14-Universal.zip
4EDF3610485E70803A4D234F4F3EA18D9E4B8763B0468A514EAC6B0EAD4AFE44  RuneSchema-0.7.5.14-Core.zip
106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123  UE4SS-3.0.1-f6d5f942-Steam-GOG.zip
95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0  UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip
```
