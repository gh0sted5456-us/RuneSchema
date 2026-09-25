# RuneSchema 0.7.5.26 artifacts

Use the universal package for the normal installation. The same `main.dll`
selects the Steam/GOG or Game Pass/WinGDK lane at runtime. The core package is
the same runtime without optional plugins.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `RuneSchema-0.7.5.26-Universal.zip` | 3,891,463 | `E4DA966F780C6A07938A875C3BE6C450147A3B19DAA97F56FE14520BE3CD1920` |
| `RuneSchema-0.7.5.26-Core.zip` | 3,415,820 | `55847DAB819AB51B73087D72475C09F45004ED59480A33F409A758AFE9BB8B3A` |
| `UE4SS-3.0.1-f6d5f942-Steam-GOG.zip` | 8,541,911 | `106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123` |
| `UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip` | 8,542,528 | `95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0` |

Both RuneSchema packages include `RuneSchema/enabled.txt`, the optional USMAP,
and an empty `RuneSchema/mods` directory. Extract over an existing RuneSchema
install so user mods, settings, and the SafeClean ownership snapshot remain in
place.

Journal/lore, recipe, and automatic character-customization persistence are
disabled by default. Their loaders remain enabled. Game Pass SafeClean retains
its previous ownership snapshot until provider-backed item/recipe cleanup is
read-back verified; it never edits Xbox WGS as loose JSON.
