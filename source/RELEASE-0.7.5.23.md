# RuneSchema 0.7.5.23

This release keeps the unified Steam/GOG and Game Pass runtime introduced by
the earlier 0.7.5 builds and adds the remaining verified WinGDK journal and
wearable-mesh native contracts.

## Compatibility

- Adds dedicated Game Pass/WinGDK journal hierarchy insertion, all three
  recipe-category lookups, and hierarchy-builder layout signatures.
- Keeps the Steam/GOG native contracts isolated from the Game Pass lane.
- Uses the verified Game Pass wearable-mesh hook without enabling unverified
  Game Pass appearance hooks.
- Treats unresolved native contracts as local feature degradation so unrelated
  loaders can continue.

## Logging

- Keeps warnings, errors, summaries, and representative success records.
- Caps repetitive successful recipe, asset-clone, and Blueprint detail lines,
  then reports how many equivalent details were omitted.
- Consolidates duplicate mod-discovery and pak-directory success chatter.
- Retains character-preview diagnostics needed to investigate appearance
  problems.

## Packaging

- `RuneSchema-0.7.5.23-Universal.zip` includes the optional Helpy plug-in.
- `RuneSchema-0.7.5.23-Core.zip` proves the core runs without plug-ins.
- Both packages contain `enabled.txt`, an empty `mods` directory, mappings, and
  the same dual-lane `main.dll`.
