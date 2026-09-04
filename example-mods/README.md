# RuneSchema 0.6.1E extended-authoring examples

Copy a selected example folder into `RuneSchema/mods`. Each example README is a
complete field guide for its loader and includes use cases and testing notes.

- `RuneschemaCapes`: `$Clone`, `/assets`, `/raw`, `/recipes`, identities, reflected
  fields, crafting, and deterministic compatibility/balance patches.
- `FourPlayerProfiles`: every `/players` selector, multiplier, exact attribute
  operation, appearance field/source/fallback, range, conflict, and party use case.
- `NamedSpawnShowcase`: every field accepted by the retained 0.6.2 `/spawns`
  loader, including per-instance `DisplayName`.
- `DawnveilPaladinSet`: a four-piece cloned equipment set combining Paladin plate,
  a Shadowscale hood, a Saradominist cloak, private wearable ratings, Armour Bench
  recipes, and a native equipment-safe gameplay effect.

## Settings and deterministic mod order

The build retains the expanded `config/config.json` settings UI/configuration,
including automatic reload, debug logging, experimental spawn drop scaling,
tooling, schema generation, FModel snippets, and `mods.txt` management switches.

`RuneSchema/mods/mods.txt` is the authoritative enabled order. A typical file:

```text
RuneschemaCapes=1
CapeBalancePatch=1
FourPlayerProfiles=0
```

`1` enables a mod and `0` disables it. When configured, RuneSchema creates the
file, reconciles folders, preserves comments, and enforces strict `0`/`1` values.
The control panel can enable, disable, and reorder entries.

Determinism is applied at three levels:

1. Enabled mod folders follow `mods.txt` order.
2. Loader files within a folder are processed in stable sorted path order.
3. `$Patch` uses deterministic recursive merge behavior; later ordered writes win.

Use this to build layers: content pack, balance pack, server rules, compatibility
patch. Keep new asset persistence IDs/internal names, recipe IDs, and DataTable rows
unique even when deterministic ordering would otherwise resolve field conflicts.

The examples are shipped outside the live `RuneSchema/mods` directory so installing
the DLL does not silently activate gameplay changes.

Credits: Okaetsu (PalSchema), Snorkles (RuneSchema), and Jonesing4Space.
