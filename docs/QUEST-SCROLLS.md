# Craftable quest scrolls

Alpha contract: a real cooked or permanent RuneSchema-cloned consumable ItemData
can accept an existing RuneSchema `/quests` definition when used. It is a normal
item with its own stable PersistenceID, so ordinary station `/recipes` can
produce it. The station needs no dialogue, and the quest remains a separate
per-character native quest record. This does not embed an executable quest or
grant rewards directly from an item.

## Item-side declaration

In `mods/JobBoard/assets/20-QuestScrolls.jsonc`:

```jsonc
{
  "/Game/Mods/JobBoard/Items/ITEM_RatContract.ITEM_RatContract": {
    "$QuestScroll": {
      "QuestID": "Dye:rat_work",
      "PersistenceID": "VT39acMY4k62LnArwSMkEQ"
    }
  }
}
```

Replace the path, QuestID and sample identity with your actual item/quest.
`PersistenceID` inside `$QuestScroll` is an expected identity check, **not an
identity rewrite**: it must already match the item's canonical 22-character
PersistenceID. Give the item and quest different stable IDs. Local QuestIDs
resolve within the mod containing the declaration; `Mod:Id` references another
installed `/quests` definition. Optional `EntryID` chooses one existing quest
entry option, including its normal cost. No implicit choice is made.

The cooked item must derive from `/Script/Dominion.ConsumableItemData` and
**explicitly override the native UsableItemInterface `UseItem` event in a
Blueprint**, with an inert body returning false and `BlueprintPure=false`.
Use remains an action, not a pure expression that Blueprint can reevaluate.
Native-only ConsumableItemData
is not a supported route: its interface implementation can execute directly in
C++ and bypass the pre-consumption event. Interface-inclusive reflection lookup
alone does not establish an actual Blueprint override. Binding rejects a missing
script override. One reusable cooked class can serve every scroll; you do not
need a new Blueprint class per quest. Configure its normal icon, text, scroll appearance, use verb and
ordinary item identity in the PAK; alternatively make a permanent `/assets`
`$Clone` with a new item PersistenceID/InternalName and this directive. A clone
must finish native item registration before it can activate. A registration or
identity error is not bypassed by the directive. Restart after changing it.

Opting in **replaces** the item's `UseItem` body. Do not also wire quest grants,
item removal, rewards, recipe unlocks or other effects into a cooked
`OnConsumeSuccess`/`UseItem` callback: that body is not run in addition. Other
consumables without this declaration are untouched. This contract activates
RuneSchema quests, not arbitrary cooked QuestData or client-supplied code.

## Schema-only scrolls with the included helper

The included content plugin is `RuneSchema.QuestItems`,
with the reusable class
`/RuneSchemaQuestItems/Blueprints/BP_QuestScroll.BP_QuestScroll_C` and a benign
`/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template.ITEM_QuestScroll_Template`
ItemData instance. It is enabled in the combined candidate's `plugins.txt` and
requires no extra native DLL or modified UE4SS. The older runtime-only r4 ZIP
does not contain it. A mod can author a scroll without its own PAK:

```jsonc
{
  "/Game/RuneSchema/MyQuests/Items/rat_contract.rat_contract": {
    "$Clone": "/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template.ITEM_QuestScroll_Template",
    "PersistenceID": "VT39acMY4k62LnArwSMkEQ",
    "InternalName": "rat_contract",
    "Name": "Rat Contract",
    "FlavourText": "Read to accept the rat-hunting job.",
    "$QuestScroll": {
      "QuestID": "Dye:rat_work",
      "PersistenceID": "VT39acMY4k62LnArwSMkEQ"
    }
  }
}
```

Generate a unique ID for your clone; the same value appears in both identity
and expected-identity fields above. Keep it stable across updates. Recipes
reference the resulting `/Game/RuneSchema/MyQuests/Items/...` ItemData path.
The shared template has no bound quest or reward and its fallback use is inert.

The identity-correction candidate distinguishes an item's live asset identity
from its nonreplicated serialization cache. New native Items may have an empty
`ItemDataPersistenceId`; the cache is not rewritten. The bound asset ID and
owned inventory slot remain mandatory, and any populated cache must agree.
See [identity diagnostics and remaining live tests](QUEST-SCROLL-IDENTITY-DIAGNOSTIC.md).

## Recipe output

Use the same item path in an ordinary recipe's output:

```jsonc
{
  "RECIPE_RatContract": {
    "Unlock": true,
    "ItemsConsumed": [
      { "ItemData": "/Game/YourMod/Items/ITEM_Paper.ITEM_Paper", "Count": 1 }
    ],
    "ItemsCreated": [
      { "ItemData": "/Game/Mods/JobBoard/Items/ITEM_RatContract.ITEM_RatContract", "Count": 1 }
    ],
    "AddTo": [{
      "DataTable": "/ColorsOfMoney/Core/JobBoard/Data/DT_QuestBoard.DT_QuestBoard",
      "Row": "QuestBoard",
      "Category": "Contracts"
    }]
  }
}
```

These are authoring placeholders, not an installed working mod. Replace the
paper asset and use the actual station row's supported category/array layout.
Recipes use normal registration/unlock rules; the scroll is not a recipe
unlocker in disguise. Quest Board's cooked content is maintained separately.

## Authority and failure behavior

The pre-consumption native `UseItem` route is intercepted only for declared
scrolls. RuneSchema validates the reflected argument types, live gameplay
controller, loaded main inventory, owned slot, exact ItemData and saved item
identity. A remote client can forward only its ordinary native inventory/slot
RPC; the authority selects the quest from its own declaration. There is no
client-supplied QuestID, reward, function or asset path in that request. This
initial contract does not consume from chests or another player's inventory.

The existing quest acceptance receipt checks prerequisites, time, explicit
entry choice, active/completed state, repeat schedule and all required counts.
It confirms the native Given state before charging one scroll plus any normal
start cost. Same-item costs are combined. Active/cooldown/prerequisite/missing
quest and ordinary readiness rejection do not consume a scroll. Reentrant
acceptance is blocked per character; active/pending receipts are not replayed.
Rewards still go through normal quest completion and its one-reward-per-run
receipt, never through scroll use.

An exceptional partial inventory/write failure after acceptance leaves the
existing pending/uncertain receipt for diagnosis. RuneSchema does not promise
cross-item atomic removal, automatically replay charges, invent a refund or
award a reward. Correct the underlying fault and inspect the native state
before recovery. Stock UE4SS is not modified.

## Automatic Quest Cancellations

RuneSchema automatically creates a **Quest Cancellations** crafting category on
the Quest Board. Each recipe placed in `DT_QuestBoard`, row `QuestBoard`, whose
output is a registered acceptance `$QuestScroll`, gets a matching **Cancel:
<quest title>** scroll. It applies to enabled mods using that contract, including
future quests, without authors writing a second item or recipe. Unrelated
crafting stations and ordinary items do not create cancellation entries.

There is one cancellation item and recipe per qualified parent quest, even
when multiple recipes or entry choices accept it. Its stable item/recipe IDs
derive from the parent quest key and survive title, icon, and recipe changes.
The scroll inherits its parent's icon and appearance, costs one ash log to
craft, and consumes one scroll only after a confirmed cancellation. Restart
after adding or changing board definitions. Generated cancellations never
generate further cancellations.

Read the cancellation scroll to abandon that character's active parent quest.
This clears its current objectives and map locations and cancels its running
contract encounter and temporary enemies. It grants no reward and refunds no
accepted supplies or entry cost. Ungiven, completed, and pending/uncertain runs
are left alone and consume no scroll. Other characters' quests and unrelated
encounters are unchanged. A partial native/write/removal failure is reported;
inventory operations are not automatically replayed or refunded.
Paid completion history and its timestamp survive cancellation; retrying does
not restore first-run rewards or retain the cancelled attempt's staged credit.

For a manually authored cancellation scroll use the same item-side declaration
with `"Action": "Abandon"`. Omitted `Action` retains the existing `"Accept"`
behavior. `EntryID` is valid only for acceptance. Cancellation uses the same
owned inventory slot, identity, authority and native inventory RPC checks as
acceptance; clients cannot choose another quest or action over that RPC.

The automatic category targets RuneSchema-bound acceptance items on the named
Quest Board. An unrelated cooked quest item must first expose a supported
`$QuestScroll` binding; RuneSchema does not infer quests from display names.

## Validation boundary

The isolated menu-only reflection probe confirmed current native signatures
for `UsableItemInterface:UseItem`, `CanUseItem`, the owned inventory use RPC,
and inventory-slot accessors. It invoked none of those functions and opened no
world. Pure tests exercise declaration validation, identity conflicts and the
real acceptance/reward receipt state machine. Full cooked-scroll consumption,
save/reload, controller input and dedicated/co-op acceptance still require live
testing. Menu reflection alone is not evidence that these workflows passed.
The shared plugin's two packages decode with both storefront mappings. Its
editor fallback returns false with no inventory, and the cooked UseItem is
const, scripted and nonpure with the expected parameters. Editable sources and
the portable container receipt are preserved under
`source/content/RuneSchemaQuestItems`; packaging verifies the container hashes.
The Quest Board provider's old native-only prototype is not approved for use.
