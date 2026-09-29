# RuneSchema 0.7.9

RuneSchema 0.7.9 uses one runtime for Steam/GOG and Game Pass/WinGDK.

## Packages

| Package | Bytes | SHA-256 |
|---|---:|---|
| `RuneSchema-0.7.9-Core.zip` | 3440565 | `75C245D908CC8A38CDFDAE7DF895E32B9C4EB04EA73E3E61E252D2108B5DDB77` |
| `RuneSchema-0.7.9-Universal.zip` | 3915425 | `9EF75607EF7E4D26DD4F65F0E4AE72CAC99C94B4FEA22CF2FDC78C63001C2B05` |

## Notes

- One RuneSchema content definition is used in standalone and multiplayer.
- NPC definitions no longer use a `Multiplayer` field.
- Steam/GOG and Game Pass/WinGDK keep separate native binding lanes selected at runtime.
- Runtime item clones complete native ItemSubsystem registration before inventory and recipe use.
- Normal RuneSchema recipe unlocks may persist once the live RecipeData has a valid PersistenceID.
- Generated vendor recipes remain transient.
- Safe Clean removes orphaned RuneSchema identities after content registration.
- Startup logs continue to report RuneSchema version, storefront, native lane, mapping status, and network role.

Universal includes the optional bundled plugins. Core omits them.
