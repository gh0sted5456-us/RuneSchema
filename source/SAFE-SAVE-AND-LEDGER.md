# Save cleanup

RuneSchema does not keep a player-content ownership ledger or restore history.

The cleanup order is deliberately small:

1. Load enabled mods and their cooked content.
2. Register all live item, recipe, quest, and journal identities.
3. Before character hydration, remove saved identities that do not resolve in the completed registries.
4. Let Dragonwilds load and save the cleaned character normally.

Removing and later reinstalling a mod is treated as a fresh installation. Removed character state is not restored.

Steam/GOG and Game Pass use the same native character-load boundary. RuneSchema
does not rewrite stored Steam character files or Xbox WGS containers. It waits
for two identical complete registry captures, scans the JSON value the game is
about to load, and changes that in-memory value only when an unresolved identity
is actually removed. An unchanged character is a strict no-op. Dragonwilds then
owns normal saving through the active storefront.

World saves remain authoritative for placed structures. Dragonwilds stores a stable class GUID for each placed building piece. RuneSchema reconstructs active custom definitions in deterministic `PersistenceID` order and does not maintain a separate building manifest or retired placeholder history.
