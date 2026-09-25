# Dialogue loader

**Folder:** `RuneSchema/mods/<ModName>/dialogue/`

Conversations, choices, requirements, and actions for NPC-driven flows.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Dialogue files define one conversation with an entry node and up to four
choices per node.

```json
{
  "Id": "hello",
  "Entry": "start",
  "Nodes": {
    "start": {
      "Text": "Welcome.",
      "Choices": [
        {"Id":"trade","Text":"Show me your wares.","VendorID":"shop"},
        {"Id":"leave","Text":"Goodbye.","End":true}
      ]
    }
  }
}
```

Walkthrough:

1. Create the dialogue ID.
2. Bind it from an NPC's `DialogueID`.
3. Link choices with `Next`, or end them with `End:true`.
4. Add requirements before actions that must be gated.
5. Use supported actions for vendors, quests, events, lore, animation, and
   Niagara.
6. Test the initial prompt, every branch, and reconnect behavior.

Example: `examples/RSv16/dialogue/60-GoblinPatrol.json`.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
