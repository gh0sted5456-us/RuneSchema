# RuneSchema 0.7.5.22

Use the Universal archive for the normal installation with the bundled
optional plug-ins. Use Core for a plugin-free RuneSchema installation. Both
archives contain the same unified `main.dll` and support Steam/GOG and Game
Pass/WinGDK through separate runtime lanes.

Extract the selected RuneSchema archive into the UE4SS `Mods` directory. The
archive already contains `enabled.txt`, the mapping file, and an empty `mods`
folder.

## SHA-256

```text
RuneSchema-0.7.5.22-Universal.zip  D188EEC68A83043D4348D09A22994502C848C572D1B44B33217D0222A528333A
RuneSchema-0.7.5.22-Core.zip       2AA1EE918D5CDED315107206B67BA63504330386CB1878EC9730CC7431A802F8
UE4SS-3.0.1-f6d5f942-Steam-GOG.zip 106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123
UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip 95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0
```

The packaged DLL passed the UE4SS ABI check, UPX integrity check, and all 31
release contracts before publication. The contracts include absolute managed
scale reconciliation and the appearance-only LocalAppData snapshot boundary.
