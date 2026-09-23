# RuneSchema 0.7.5.14

- Consolidates optional mapping data under `RuneSchema/dlls/mappings` while
  retaining read compatibility with previous mapping locations.
- Stores the compact owned-content snapshot under
  `RuneSchema/settings/safesave/OwnedContentLedger.json` and migrates the old
  settings-root snapshot on first use.
- Normalizes Helpy tabs, overlays, and the item cart to the same 960 x 720
  painted shell.
- Increases Helpy text scale and adds a bounded, session-only, visible-first
  icon cache shared across all tabs. Opening Helpy still performs no full scan.
