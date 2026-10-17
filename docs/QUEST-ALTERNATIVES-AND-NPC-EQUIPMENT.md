# Alternative objectives and shared-world NPC equipment

Runtime/schema support is included in RuneSchema 0.8.0.0. Publication does not
replace live-world acceptance testing for these capabilities. Both sides of a
multiplayer session need matching framework versions and definitions.

## One quest, either route

Each stage may specify `"Completion":"Any"`; omitted/`All` retains existing
behavior. An Any stage requires 2–16 visible mandatory objectives (no Hidden or
Optional alternatives). It shares the quest's existing PersistenceID, run,
cooldown, reward receipt and native save counters. It does not create another
quest or award each route separately. Quest-level `Completion` still means
`ReturnToNPC` or `Automatic`; stage-level Completion is a different setting.

For `Dye_Quests:maxxfilth_rat_race`, author a single Any stage with:

- `rats`: Kill, Count 50, Location `[17355,171163,-185]`, RadiusMeters 300.
  COLORS must provide the verified native rat AI class; do not guess its path.
- `meat`: Collect, Count 50, Item
  `/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Raw_Stringy_Meat.ITEM_Resources_Raw_Stringy_Meat`.
- Quest `Repeatable:true`, `Repeat:{"CooldownSeconds":300}` and the author's
  chosen Reward. No reward amount is imposed by the loader.

Collect checks already-carried inventory at hand-in. Acquire instead tracks
positive additions after acceptance and does not consume items; do not use
Acquire for the meat route. Kill radius uses victim XY position and the existing
authenticated player/owned-helper credit policy, not every rat killed globally.

The first server-processed completed counter wins. A completed kill route wins
over uncommitted inventory even when the player carries enough meat. Otherwise,
the first eligible authored hand-in alternative is selected and only its full
amount is consumed. One event cannot complete two alternatives or a successor
stage. Pending exchanges retain the existing fail-closed recovery semantics;
an uncertain inventory operation is never automatically charged or rewarded
again. Reacceptance after the cooldown resets all routes for the next run.

Existing definitions without the new field keep their identities and behavior.
Changing an already-active All quest into Any is a gameplay-definition change,
not a presentation-only edit; the existing receipt migration policy applies.
The Maxxfilth drafts were never installed and have no legacy progress to migrate.

## A persistent equipment consequence

Add this to the Human NPC definition (not the quest):

```jsonc
"EquipmentWhen": [{
  "Id": "chrome_helmet",
  "QuestCompleted": "Dye_Quests:maxxfilth_missing_piece",
  "Policy": "WorldEverCompleted",
  "Equipment": {
    "Head": "/ColorsOfMoney/Armor/Chrome/Items/ITEM_colors_of_money_item_armour_t5_head_skeleton_Chrome.ITEM_colors_of_money_item_armour_t5_head_skeleton_Chrome"
  }
}]
```

Initial Equipment contains Body/Legs and no Head. Completion adds the helmet,
preserving those other slots, the actor, its interactions and current animation.
The final one-time quest's ordinary Collect objective consumes one helmet and
its ordinary Reward gives 10
`/ColorsOfMoney/Core/Data/Items/ITEM_Currency_Gold.ITEM_Currency_Gold`.
The equipment rule itself grants no currency or items.

Policy is required. `WorldEverCompleted` means any authoritative completion in
this world while the rule is enabled permanently latches the shared visual for
everyone. It is not a currently-connected-player check. A character bringing a
completed quest from another world cannot latch this world's rule. Per-player
presentation, arbitrary condition scripts, weapon/pose changes, removal rules
and cooked-only quests are not supported by this contract. QuestCompleted
references an enabled RuneSchema quest ID, local or `Mod:Id`.

Rules have unique stable IDs, at most 16 per NPC, and nonoverlapping wearable
slots (Necklace/Trinket2 are the same slot). Base Equipment can be overridden;
two conditional rules cannot fight over one slot. Equipment values accept item
paths or canonical PersistenceIDs and must pass the native wearable-slot check.
Changing the quest associated with a latched rule is rejected; use a new rule ID.

Authority atomically persists the rule latch under
`RuneSchema/runtime/live/saved/progress/worlds/<native-WorldSaveGuid>/npc-equipment.json`.
The world GUID is read from reflected PersistenceSubsystem settings, never a
display name, network session ID or executable offset. The file is a RuneSchema
sidecar, not an edit to the game's world save. Back up/restore/move it together
with that world; restoring only an older game save does not rewind an ever-latch.
Malformed or world-mismatched ledgers are preserved and reported, not reset.
Unloading the NPC/mod hides its actor without deleting its latch or rewarding it.

The quest's native record also stores completion-world proof, allowing bounded
authority recovery of a failed sidecar write without adopting another world's
progress. No polling path gives quest rewards. Server-authoritative rule IDs
(not arbitrary asset paths) travel in the existing replicated NPC identity.
Client loadouts are derived from matching local definitions. Dedicated servers
maintain the latch and replication without constructing character previews.

## Live acceptance still required

Verify both routes separately, both-ready precedence, repeat/reload, inventory
full/pending recovery, unrelated players, then final helmet hand-in and 10-coin
reward. Verify the helmet immediately, after the completer disconnects, late
join, cell unload/reload and a dedicated-server restart without that character.
Check a second world stays bare-headed. Steam/Game Pass reflection decoding and
native loadout presentation need in-game confirmation; unit tests are not proof
of a successful live hand-in or visible helmet.
