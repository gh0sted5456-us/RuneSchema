# Save cleanup

RuneSchema uses the live game registries as the only authority for player-save cleanup. It does not maintain an ownership ledger, content manifest, or restore history.

All enabled mods load first. RuneSchema then registers the available item, recipe, quest, and journal identities. Immediately before character hydration, unresolved saved identities are removed and the game continues with the cleaned character.

Steam/GOG uses an atomic JSON replacement with a backup. Game Pass applies the same validation at the native JSON boundary without editing Xbox WGS containers.

Reinstalling removed content is a fresh installation; previously pruned state is not restored.

World saves remain authoritative for placed structures. RuneSchema keeps vanilla building order and registers active custom definitions in deterministic `PersistenceID` order. It does not create a separate building manifest or retired-placeholder history.
