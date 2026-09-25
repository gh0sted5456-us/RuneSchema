# Quests loader

**Folder:** `RuneSchema/mods/<ModName>/quests/`

Per-character quest definitions, objectives, rewards, and persistence.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Quests are per-character definitions with stages, objectives, rewards, and a
stable persistence ID.

```json
{
  "Id": "gather_logs",
  "PersistenceID": "stable-quest-id",
  "Title": "Gather Logs",
  "Description": "Bring back five logs.",
  "Completion": "ReturnToNPC",
  "Stages": [{
    "Id": "gather",
    "Objectives": [{
      "Id": "logs",
      "Type": "Fetch",
      "Item": "/Game/Gameplay/Items/ITEM_Log.ITEM_Log",
      "Count": 5
    }]
  }],
  "Reward": {"Item":"/Game/Gameplay/Items/ITEM_Reward.ITEM_Reward","Count":1}
}
```

Walkthrough:

1. Assign stable quest and persistence IDs.
2. Define stages and unique objective IDs.
3. Bind accept and turn-in actions from dialogue.
4. Add event IDs or search areas to kill objectives when required.
5. Test the acceptance toast, progress, reconnect, reload, turn-in, repeat, and
   removal behavior.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
