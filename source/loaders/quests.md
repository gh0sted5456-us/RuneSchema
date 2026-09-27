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

## Simple rules

- Quests are per-character definitions with stable quest and persistence IDs.
- Stage objective IDs must be unique within the authored quest.
- Bind accept and turn-in actions through dialogue, then test persistence across reconnect and reload.

## FAQ

### FAQ-QUESTS-001 — Are RuneSchema quests global or per character? {#faq-quests-001}

They are per-character quest definitions.

### FAQ-QUESTS-002 — Do I need a stable PersistenceID? {#faq-quests-002}

Yes. Use a stable persistence ID so the same authored quest keeps the same
persistent identity across loads and updates.

### FAQ-QUESTS-003 — Where do quest accept and turn-in actions come from? {#faq-quests-003}

Bind them from supported dialogue actions.

## Working examples

- [Great Tree: multi-stage story quest](../examples/GreatTree/quests/87-GreatTreeStory.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
