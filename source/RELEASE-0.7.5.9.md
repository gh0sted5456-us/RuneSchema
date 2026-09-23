# RuneSchema 0.7.5.9

This release adds verified cooked-content ownership declarations for PAK authors.

Place `$declaration` in a JSON or JSONC file under `/assets`, `/buildings`, `/quests`, `/journal`, or `/lore`. It may be one object or an array. `/assets` requires `Kind`; the other folder determines it automatically:

```json
{
  "$declaration": [
    {
      "Kind": "Item",
      "Path": "/Game/Mods/MyMod/Items/ITEM_MySword.ITEM_MySword",
      "PersistenceID": "0123456789abcdefghij_A"
    },
    {
      "Kind": "Recipe",
      "Path": "/Game/Mods/MyMod/Recipes/REC_MySword.REC_MySword",
      "PersistenceID": "0123456789abcdefghij_Q",
      "InternalName": "REC_MySword"
    }
  ]
}
```

The same cooked quest under `/quests` is shorter:

```json
{
  "$declaration": {
    "Path": "/Game/Mods/MyMod/Quests/QUEST_Castle.QUEST_Castle",
    "PersistenceID": "0123456789abcdefghij_g"
  }
}
```

- The directive does not clone or patch the cooked object.
- RuneSchema loads the declared object, verifies the loader-specific native class and baked `PersistenceID`, and reads its actual `InternalName`. Supplying `InternalName` makes it an additional assertion.
- Verified declarations enter the same historical owned-content ledger used by RuneSchema clones.
- Active declared assets are admitted to the appropriate native registry even when their mount path is outside `/Game/Mods`.
- When the declaring mod is disabled or removed, RuneSchema uses the subsystem's existing safe cleanup: native inventory removal for items, both progress unlock sets for recipes, historical network slots for buildings, owned progress rows for quests, and the journal ownership envelope for journal/lore entries.
- The tag does not automatically unlock a cooked building, quest, journal entry, or lore entry. Its purpose is ownership, registration, and safe retirement.
- Invalid declarations are isolated while ordinary records continue loading.

Runtime-only content such as effects, Niagara, vendors, NPCs, and spawns has no persistent DataAsset identity to declare and therefore does not use `$declaration`.
