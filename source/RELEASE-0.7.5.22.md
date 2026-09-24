# RuneSchema 0.7.5.22

## Persistent authored scale

- Reconciles RuneSchema-managed NPCs, resources, AI spawn points, actors, static
  assemblies, buildings, and player scale rules once per second.
- Treats authored scale as an absolute value. RuneSchema never multiplies a
  scale restored by the save or native respawn system, preventing cumulative
  growth and shrinkage across reloads.
- Extends the 0.7.5.21 resource-node correction to managed content such as
  Bloodwood sap choppables, Edna, Pete, books, and other author-selected actors.
  Content that is not owned or declared by RuneSchema is not rewritten.

## Appearance-only player fallback

- Creates one write-once snapshot for each character GUID at
  `%LOCALAPPDATA%\RSDragonwilds\Saved\RuneSchema\players\<guid>.json`.
- Captures body, face, hair, facial hair, skin tone, hair color, eye color, and
  eyebrow color only. It does not capture armor, equipment, inventory, stats,
  quests, player names, or general save data.
- Captures the snapshot before RuneSchema applies an appearance rule and never
  refreshes it during normal play, so it remains a stable fallback rather than
  an appearance-changing system.
- Stores appearance ownership beside the snapshots. If an appearance selection
  applied by RuneSchema belongs to a mod that is later missing or disabled, the
  affected field is restored to its recorded fallback. If that recorded row is
  also unavailable, RuneSchema retries with the original player snapshot.
- Migrates the earlier `appearance-fallbacks.json` ownership file into the new
  LocalAppData player folder without deleting the legacy copy.

## Compatibility and safety

- Keeps the unified Steam/GOG and Game Pass runtime lanes from 0.7.5.21.
- Snapshot capture is authority-only, bounded to one check per second, and uses
  atomic temporary-file replacement.
- Failures defer only the affected snapshot or appearance field; unrelated
  loaders and mods continue.
