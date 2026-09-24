# RuneSchema 0.7.5.21

Use the Universal archive for the normal installation with the bundled
optional plug-ins. Use Core for a plugin-free RuneSchema installation. Both
archives contain the same unified `main.dll` and support Steam/GOG and Game
Pass/WinGDK through separate runtime lanes.

Extract the selected RuneSchema archive into the UE4SS `Mods` directory. The
archive already contains `enabled.txt`, the mapping file, and an empty `mods`
folder.

## SHA-256

```text
RuneSchema-0.7.5.21-Universal.zip  AF3DC6016A78E701F4B30DC5DCF164198CBBF1EBB825A28D2A17E1432CD26AB3
RuneSchema-0.7.5.21-Core.zip       F29F1B2D7E3E98C9438750872D59415B7DF61E7EE3B7E2D067BE9CCEE7A37FF6
UE4SS-3.0.1-f6d5f942-Steam-GOG.zip 106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123
UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip 95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0
```

The packaged DLL passed the UE4SS ABI check, UPX integrity check, and all 31
release contracts before publication, including the resource-scale
idempotence contract.
