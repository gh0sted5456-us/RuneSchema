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

## Simple rules

- One dialogue definition has an entry node and up to four choices per node.
- Bind the dialogue from an NPC's `DialogueID`.
- Gate actions with requirements, then use supported actions for vendors, quests, events, lore, animation, or Niagara.

## FAQ

### FAQ-DIALOGUE-001 — Can one dialogue node have more than four choices? {#faq-dialogue-001}

No. The current dialogue contract supports up to four choices per node.

### FAQ-DIALOGUE-002 — Can dialogue open vendors or drive quests and events? {#faq-dialogue-002}

Yes, through the supported dialogue actions. Bind the conversation to an NPC,
then use the appropriate vendor, quest, event, lore, animation, or Niagara action.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
