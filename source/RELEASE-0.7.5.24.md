# RuneSchema 0.7.5.24

This is a narrow Game Pass/WinGDK crash correction over 0.7.5.23.

## Fixed

- Replaces the incorrect WinGDK journal hierarchy insertion candidate at
  `0x76C86F0` with the verified builder call target at `0x6ED8FD0`.
- Matches the journal map's 40-byte soft-object key and 28-byte hierarchy value.
  The rejected candidate expected an unrelated 64-byte key and 8-byte value,
  which caused an invalid native memory copy during journal/lore startup.
- Adds a release contract that rejects the unsafe map-specialization signature.

## Preserved

- The Steam/GOG native lane is unchanged.
- The 0.7.5.23 WinGDK wearable-mesh and character-mapping corrections remain
  enabled and unchanged.
- All category lookups, hierarchy-layout validation, loader isolation, Helpy,
  mappings, SafeSave, scale reconciliation, and packaging behavior are retained.

## Packages

- `RuneSchema-0.7.5.24-Universal.zip` includes the optional Helpy plug-in.
- `RuneSchema-0.7.5.24-Core.zip` contains the plugin-free core.
- Both packages contain `enabled.txt`, an empty `mods` directory, mappings, and
  the same dual-lane `main.dll`.
