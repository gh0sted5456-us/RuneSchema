# Quests loader

**Folder:** `RuneSchema/mods/<ModName>/quests/`

Per-character quest definitions, objectives, rewards, and persistence.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Quests are per-character definitions with stages, objectives, rewards, and a
stable persistence ID.

Quests can also be accepted through a craftable consumable item using an
item-side [`$QuestScroll` declaration](../../QUEST-SCROLLS.md) in `/assets`.
The recipe produces a normal stable-ID item; use activates an existing quest
through the authoritative acceptance receipt, without requiring station dialogue.

```json
{
  "Id": "gather_logs",
  "PersistenceID": "AAAAAAAAAAAAAAAAAAAAAA",
  "Title": "Gather Logs",
  "Description": "Bring back five logs.",
  "CompletedDescription": "The village has the timber it needs.",
  "Region": "Brynmoor",
  "ShowInCompletedQuestList": true,
  "Category": "Regular",
  "Completion": "ReturnToNPC",
  "Stages": [{
    "Id": "gather",
    "Objectives": [{
      "Id": "logs",
      "Type": "Collect",
      "Text": "Bring back five logs.",
      "Hint": "Look near the forest edge.",
      "Item": "/Game/Gameplay/Items/ITEM_Log.ITEM_Log",
      "Count": 5
    }]
  }],
  "Reward": {"Item":"/Game/Gameplay/Items/ITEM_Reward.ITEM_Reward","Count":1}
}
```

Replace the example item paths with your cooked `ItemData` paths and generate a
unique, canonical 22-character `PersistenceID` for each quest. Keep that identity
stable when updating the same quest.

## Quest lists and completion

7.8 alpha stages also support `Completion: "Any"` for alternative objectives
with one completion/reward receipt. See [alternative objectives and NPC
equipment consequences](../../QUEST-ALTERNATIVES-AND-NPC-EQUIPMENT.md) for the
kill-or-hand-in contract, limits, repeat behavior and live-validation status.

A file may contain one definition or an array of definitions. Multiple quests
remain separate per-character native progress records; accepting another quest
does not complete the first. `Category` accepts `Regular`, `Story`, or `Task`.
The game maintains its own tracked-quest selection by quest type; tracking is
not the same as which quests a player owns or has completed.

`CompletedDescription` controls the native description after completion and
defaults to `Description`. `ShowInCompletedQuestList` defaults to `true` and
controls completed-list visibility only: it never completes the quest or grants
its reward. `HideInQuestList` defaults to `false` and controls the normal list.
Internal hidden stage records remain hidden from both lists.

Completion still follows the authored `Completion` rule and native quest state.
Rewards and repeat eligibility remain separate from list presentation. Hiding
an entry does not remove its persisted progress.

## Quest region

`Region` is the game's native `EQuestRegion` name. Current internal names are
`Brynmoor`, `Ghornfell`, `Fellhollow`, `Dowdun_Reach`, `Umbral_Sands` and
`Scorned_Wilderness`. `EQuestRegion::Brynmoor` is also accepted. Names are resolved
from the running game's reflected enum, not hard-coded numeric IDs or a fixed
allowlist. A future region added to that same enum can be used in JSON without
updating RuneSchema. An unavailable name is rejected with a targeted error;
`None`, `MAX` and numeric IDs are not accepted.

Omitting `Region` preserves legacy behavior, which can display the game's
missing-region placeholder. RuneSchema does not infer a region from coordinates.
An arbitrary custom label is not a new native map region: that needs separate
UI/world support. Quest category, region and objective search radius are distinct.

Presentation-only updates preserve native phase, counters, repeat run and reward
receipts after the progress fingerprint has been recorded. Old full-document
receipts can migrate proven additions of `Region` and/or `CompletedDescription`
without replaying rewards or resetting progress. Other old presentation edits
cannot be proven from the old hash alone; migrate to the new receipt before
changing those fields. Changes to identity, objectives, rewards, costs or repeat
rules remain gameplay changes and retain their migration/reset checks.

## Objective presentation

Objectives support `SubobjectiveDescription`, `Hint`, and
`QuestDescriptionOverride` in addition to `Text`. These map to the game's
structured objective data. A description override applies when that objective
is selected by native progression; it does not overwrite other quests.

For staged quests, visible objective descriptions, subobjective text and hints
are combined for the active stage. Hidden objective presentation is excluded
from that stage summary. `CompleteText` remains progress text, not a replacement
for the quest-level `CompletedDescription`.

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

- [Great Tree: multi-stage story quest](../../examples/GreatTree/quests/87-GreatTreeStory.json)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
