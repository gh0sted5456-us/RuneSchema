# RuneSchema 0.7.5.27 artifacts

Use `RuneSchema-0.7.5.27-Universal.zip` for the normal installation. It
contains the optional plugins. Use the core archive only when deliberately
testing RuneSchema without plugins. Install the UE4SS archive matching the
storefront, then place the selected RuneSchema folder under `ue4ss/Mods`.

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `RuneSchema-0.7.5.27-Universal.zip` | 3,890,543 | `AD17630F4E3E827B76BDA5F180CD3A34F4B7D207B69D19697A42C60CCE965043` |
| `RuneSchema-0.7.5.27-Core.zip` | 3,416,130 | `882826FC08A09C81DA04E247F0BF6CAFE85C088D4CB1264599DCD83168DFEC4E` |
| `UE4SS-3.0.1-f6d5f942-Steam-GOG.zip` | 8,541,911 | `106F807E6B7782B07DFDB34342D087EF5694E68ED5C24329958AE51BCCDBB123` |
| `UE4SS-3.0.1-f6d5f942-GamePass-WinGDK.zip` | 8,542,528 | `95FFCFD63CC50E61E7432A4ABA8052C413AB8A265FC9725ADAFFD99A6547A5F0` |

Both RuneSchema packages include `RuneSchema/enabled.txt`, the optional USMAP,
and an empty `RuneSchema/mods` directory. The universal package also includes
the optional Helpy plugin. DLL compression was verified by the build script;
signing was skipped because no signing certificate was configured.

Game Pass SafeClean uses the WinGDK provider lane and never falls through to
Steam's loose character-save directory. Version 0.7.5.27 allows supported
item/recipe cleanup to complete even while another provider category remains
pending; the previous ownership snapshot stays available for retry.
