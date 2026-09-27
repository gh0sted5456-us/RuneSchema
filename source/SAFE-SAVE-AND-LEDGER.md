# Save cleanup

RuneSchema does not keep a player-content ownership ledger or restore history.

The cleanup order is deliberately small:

1. Load enabled mods and their cooked content.
2. Register all live item, recipe, quest, and journal identities.
3. Before character hydration, remove saved identities that do not resolve in the completed registries.
4. Let Dragonwilds load and save the cleaned character normally.

Removing and later reinstalling a mod is treated as a fresh installation. Removed character state is not restored.

Steam/GOG character JSON is repaired atomically with a backup before replacement. Game Pass uses the same registry decision at the native character JSON boundary; RuneSchema does not rewrite WGS containers directly.

World saves remain authoritative for placed structures. Dragonwilds stores a stable class GUID for each placed building piece. RuneSchema reconstructs active custom definitions in deterministic `PersistenceID` order and does not maintain a separate building manifest or retired placeholder history.
