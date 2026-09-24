# RuneSchema 0.7.5.17

- Preserves the working character-menu replay and direct cooked-building
  lifetime fixes from 0.7.5.16.
- Generalizes verified character-option appends across all nine native menu
  categories and documents the complete authoring contract.
- Keeps the expanded `/assets` registry patches and `/raw` DataTable authoring.
- Makes `/registry`, `/players`, and `/nameplates` recursive, completing nested
  JSON/JSONC organization across mod loaders.
- Locks vendor filtering to category identity. Power-level, time-of-day, and
  quest gates remove unavailable categories without shifting their offers.
- Standardizes gameplay actor and placement scale authoring at 0.01 through
  100 for NPCs, vendors, spawns, events, assemblies, player rules, and Helpy.
- Uses normal mod folders only. No `.rspack` subsystem is included.
- Moves SafeSave's owned-content snapshot to
  `%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\safesave`, beside but separate
  from the existing per-world building registry data. The old installed-mod
  ledger is staged, verified, and migrated automatically.
- Keeps Steam/GOG and Game Pass/WinGDK in explicit native-binding lanes, with
  regression coverage preventing WinGDK or packaged execution from entering
  the Steam embedded-signature lane.
