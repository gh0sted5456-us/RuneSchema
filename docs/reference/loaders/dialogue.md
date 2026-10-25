# Dialogue loader

## Step-by-step workflow

Examples with placeholder paths/IDs are templates. Use inspected values and the
[shared testing checklist](../../modders/testing-and-compatibility.md) before sharing.

1. Choose a compatible interactable NPC or dialogue prop and a stable conversation ID.
2. Create `dialogue/10-camp-conversation.jsonc` with an Entry naming an existing node.
3. Add up to four choices per node; connect Next/End, then add requirements before supported actions.
4. Bind the ID through the actor's DialogueID. Restart and check every reachable branch, unmet requirement and player-specific result.

**Tie-ins:** [NPCs](npc.md), [vendors](vendors.md), [quests and boards](../../modders/quests-and-questboards.md) and [lore](lore.md).


**Folder:** `RuneSchema/mods/<ModName>/dialogue/`

Conversations, choices, requirements, and actions for NPC-driven flows.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Dialogue files define one conversation with an entry node and up to four
choices per node.

```jsonc
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

See [quest and board dialogue tie-ins](../../modders/quests-and-questboards.md#alternative-dialogue-based-boards-and-turn-in).

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

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
