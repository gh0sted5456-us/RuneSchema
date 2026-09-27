# Save cleanup

RuneSchema uses the live game registries as the only authority for player-save cleanup. It does not maintain an ownership ledger, content manifest, or restore history.

All enabled mods load first. RuneSchema then registers the available item, recipe, quest, and journal identities. Immediately before character hydration, unresolved saved identities are removed and the game continues with the cleaned character.

Steam/GOG and Game Pass use the same native character-load boundary. RuneSchema
does not rewrite stored Steam character files or Xbox WGS containers. It waits
for two identical complete registry captures, scans the JSON value the game is
about to load, and changes that in-memory value only when an unresolved identity
is actually removed. An unchanged character is a strict no-op. Dragonwilds then
owns normal saving through the active storefront.

Reinstalling removed content is a fresh installation; previously pruned state is not restored.

World saves remain authoritative for placed structures. RuneSchema keeps vanilla building order and registers active custom definitions in deterministic `PersistenceID` order. It does not create a separate building manifest or retired-placeholder history.
