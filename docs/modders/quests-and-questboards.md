# Build a quest and put it on a Quest Board

Start with one small quest, then connect an acceptance route. `/quests` defines
the quest; it does not create a board actor, a crafting recipe or a consumable.
This walkthrough uses the current RuneSchema contracts, including the optional
Quest Board provider maintained separately from the framework.

## Choose the board you mean

| Route | What you author | What the player does |
| --- | --- | --- |
| NPC or dialogue prop | `quests`, `dialogue`, and a compatible `npc` actor | Talks to accept and, when required, turn in. |
| Crafting Quest Board | `quests`, acceptance item in `assets`, recipe in `recipes`, plus the installed board provider | Crafts a contract, then uses it from their own main inventory. |
| Custom board plugin | A board UI integrating `runeschema.tools` and an existing authoritative acceptance route | Selects a displayed quest; the server still checks eligibility. |

These routes can reference the same quest. Do not create a second quest identity
just to offer another way to accept it. A catalogue row is not acceptance
permission, and a recipe unlock is not quest acceptance.

## 1. Create and enable a small mod

Use this layout beneath the installed `RuneSchema/mods` folder:

```text
MyQuests/
  quests/10-gather-ash.jsonc
  assets/20-ash-contract.jsonc
  recipes/30-ash-contract.jsonc
```

Add `MyQuests : 1` to `mods/runeschema.txt`. Keep the folder name stable: the
qualified quest key in this example is `MyQuests:gather_ash`. An unqualified
`gather_ash` resolves inside the declaring mod; another mod must use the
qualified key. See [first-mod setup](../AUTHORING-GUIDE.md).

Download the [quest](../examples/QuestBoardWalkthrough/quests/10-gather-ash.jsonc),
[acceptance item](../examples/QuestBoardWalkthrough/assets/20-ash-contract.jsonc)
and [recipe](../examples/QuestBoardWalkthrough/recipes/30-ash-contract.jsonc)
as structural starters. **Generate your own identities before installation.**
The examples are not a certified, live-tested mod or a replacement for the
separately installed board provider.

## 2. Define the quest in `/quests`

`quests/10-gather-ash.jsonc`:

```jsonc
{
  "Id": "gather_ash",
  "PersistenceID": "h6qYUGm3Qkq7jEC8WHt1xA",
  "Title": "Timber for the Camp",
  "Description": "Acquire five ash logs after accepting this contract.",
  "CompletedDescription": "The camp has its timber.",
  "Region": "Brynmoor",
  "Category": "Task",
  "Completion": "Automatic",
  "Objective": {
    "Id": "new_ash_logs",
    "Type": "Acquire",
    "Text": "Acquire five ash logs.",
    "Item": "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash",
    "Count": 5,
    "ProgressText": "Ash logs: {current}/{required}",
    "CompleteText": "The timber is ready."
  },
  "Reward": {
    "Item": "/Game/Gameplay/Items/Resources/Mineral/ITEM_Resources_Stone.ITEM_Resources_Stone",
    "Count": 1
  }
}
```

The ash and stone paths were observed in the current Steam item catalogue;
verify them against the game/build you support. Generate a new canonical
22-character quest `PersistenceID` and keep it unchanged across updates. Do not
copy another mod's ID or use the acceptance item's identity as the quest ID.

This example deliberately uses `Acquire` and `Automatic`, so no return dialogue
is needed. `Acquire` counts positive inventory additions **after acceptance or
stage activation**, not logs already carried. It does not prove the player
chopped a tree: any supported positive inventory addition can count. It does
not consume those logs at completion.

### Choose objectives and completion deliberately

| Setting | Use it for | Important distinction |
| --- | --- | --- |
| Objective `Collect` | An inventory hand-in | Do not expect an immediate acquisition counter; test the native hand-in route. |
| Objective `Acquire` | New items added while active | Not retroactive and not a harvesting-only filter. |
| Objective `Kill` | Credited deaths matching `AIClasses` | Use actor classes, not attacks; event/spawn filters require matching provenance. |
| Quest `Completion: "Automatic"` | Completion without returning to an NPC | Still uses the normal completion/reward receipt. |
| Quest `Completion: "ReturnToNPC"` | Explicit turn-in | Provide a supported dialogue `TurnIn` action. Crafting a scroll alone supplies no turn-in UI. |
| Stage `Completion: "All"` or `"Any"` | Multiple required objectives or alternatives | Separate from quest completion. `Any` requires at least two visible, mandatory alternatives. |

Choose **either** one `Objective` **or** `Stages`, never both. Current limits are
32 stages, 16 objectives per stage and 128 total. Keep stage/objective IDs
stable. Use [the quest loader reference](../reference/loaders/quests.md) and
[alternative-objective guide](../QUEST-ALTERNATIVES-AND-NPC-EQUIPMENT.md) before
adding branching outcomes, costs or repeat rewards.

## 3. Create a real acceptance item in `/assets`

The schema-only route requires the installed `RuneSchema.QuestItems` content
plugin and its cooked template:

```jsonc
{
  "/Game/RuneSchema/MyQuests/Items/ash_contract.ash_contract": {
    "$Clone": "/RuneSchemaQuestItems/Items/ITEM_QuestScroll_Template.ITEM_QuestScroll_Template",
    "PersistenceID": "m8WyO5c2SsKnv3JX7Dq2AQ",
    "InternalName": "myquests_ash_contract",
    "Name": "Timber Contract",
    "FlavourText": "Read to accept Timber for the Camp.",
    "$QuestScroll": {
      "QuestID": "MyQuests:gather_ash",
      "PersistenceID": "m8WyO5c2SsKnv3JX7Dq2AQ"
    }
  }
}
```

Generate a second unique ID for the item and replace **both** occurrences of
its sample ID. `$QuestScroll.PersistenceID` checks the existing item identity;
it does not rewrite it. The clone must finish normal native registration.
Restart after changing scroll declarations.

No extra PAK of your own is needed when the installed helper supplies this
template. If it is absent, install the supported helper/provider combination;
a JSON path cannot manufacture its cooked class. For your own cooked scroll,
follow the exact non-pure, scripted `UseItem` override contract in
[craftable quest scrolls](../QUEST-SCROLLS.md). A native-only consumable or a
generic lore book is not an equivalent substitute. Do not add a second quest
grant/reward effect to its cooked use callback.

## 4. Add a contract recipe to the crafting board

`recipes/30-ash-contract.jsonc`:

```jsonc
{
  "RECIPE_MyQuests_AshContract": {
    "Unlock": true,
    "ItemsConsumed": [{
      "ItemData": "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash",
      "Count": 1
    }],
    "ItemsCreated": [{
      "ItemData": "/Game/RuneSchema/MyQuests/Items/ash_contract.ash_contract",
      "Count": 1
    }],
    "AddTo": [{
      "DataTable": "/ColorsOfMoney/Core/JobBoard/Data/DT_QuestBoard.DT_QuestBoard",
      "Row": "QuestBoard",
      "Category": "Contracts"
    }]
  }
}
```

The table and row above are the **specific supported Quest Board contract**, not
a universal name for any board. Install/enable its provider and ensure that
the placed board uses that station row and supports the selected category.
`Unlock: true` exposes a recipe; it neither grants nor completes the quest.
The one log used to craft the scroll is a recipe cost, separate from quest
entry costs and from the five new logs required after acceptance.

Put the recipe in `recipes`, not `quests`. Creating a recipe does not create a
board actor. For a different station, use its actual row and compatible layout;
see [stations and recipes](../STATIONS-AND-RECIPES.md).

## 5. Test the player flow

1. Restart with the mod, helper and board provider enabled. Check RuneSchema's
   log for loader, missing-package and registration errors before opening a save.
2. Use a disposable character/world with backups. Place or locate the provider's
   board and confirm **Timber Contract** appears in **Contracts**.
3. Craft one contract. Confirm the ordinary recipe input/output counts.
4. Use it from that character's main inventory. Confirm the quest enters the
   native Given/active state and only a successful acceptance consumes a scroll.
5. Acquire five **new** ash logs. Confirm the objective advances and the one-stone
   reward occurs once. Logs held before acceptance should not count.
6. Test a duplicate acceptance, reconnect, world re-entry, full restart and a
   second character. Check independent progress and no duplicate rewards.
7. Repeat on the host/server and a remote client for every multiplayer/runtime
   combination you claim to support. Schema checks and menu reflection are not
   evidence that those live tests passed.

The authority validates the player's owned inventory slot and exact item,
quest eligibility and any entry cost. Missing/prerequisite/cooldown/already
active rejections do not normally consume a scroll. Exceptional partial-write
failures keep diagnostic receipts; do not automatically refund, replay charges
or award rewards to compensate. Inspect the reported native state first.

### Cancellation scrolls

For a registered acceptance scroll output placed on the supported `QuestBoard`
row, RuneSchema generates a **Quest Cancellations** recipe/category. Authors do
not need a second acceptance definition. One cancellation is generated per
qualified parent quest; generated cancellations do not recursively generate more.

Using it abandons only that character's active quest/contract encounter after
confirmation. It grants no reward and refunds no entry costs or supplies.
Completed, inactive or pending/uncertain quests are left alone. Previous paid
completion history survives cancellation. Other stations are not implicitly
opted in. See [the full cancellation contract](../QUEST-SCROLLS.md#automatic-quest-cancellations).

## Alternative: dialogue-based boards and turn-in

An interactable NPC/compatible dialogue prop can bind a `DialogueID`. In that
conversation, acceptance uses a choice action such as:

```jsonc
{"Id":"accept_ash","Text":"I will take the timber contract.","Quest":{"Id":"MyQuests:gather_ash","Action":"Accept"},"End":true}
```

For a quest authored with `ReturnToNPC`, a separate turn-in choice can use:

```jsonc
{
  "Id": "turn_in_ash",
  "Text": "The timber is ready.",
  "RequirementUnlock": {
    "Quest": {"Id":"MyQuests:gather_ash","States":["Active"],"ObjectivesComplete":true}
  },
  "Quest": {"Id":"MyQuests:gather_ash","Action":"TurnIn"},
  "End": true
}
```

These are **choice fragments**, not standalone dialogue files. Put them inside
`Nodes.<node>.Choices`, with a valid `Entry` node; at most four choices per node.
Do not add this turn-in choice to the automatic example expecting a second
reward. Use [`dialogue`](../reference/loaders/dialogue.md) and
[`npc`](../reference/loaders/npc.md) for actor binding, and keep quest
`Prerequisites` in the quest service rather than relying only on hidden choices.

## Custom board developers: names and icons

The `runeschema.tools` service accepts this read-only presentation request:

```jsonc
{"Action":"QuestCatalog","Mod":"MyQuests","Offset":0,"Limit":64}
```

Wait for `Ready`, render `Quests`, follow `NextOffset` while `HasMore`, and refresh
when `Revision` changes. The current published catalogue is bounded to 128
authored quest entries; `Limit` is 1–128 and `Offset` 0–128. Optional qualified
`Key` narrows a request. Rows include `Key`, `Mod`, `PersistenceID`, `Title`,
`Description`, `Icon`, `Category`, `Region` and `Repeatable`; internal dialogue
state rows are excluded.

Quest `Icon` is an optional mounted Unreal texture object path, not a PNG
filename or web URL. Keep fallback artwork when absent/unloadable. The catalogue
does not provide per-player progress or authorize acceptance, and RuneSchema
does not automatically replace a third-party board UI.

## Troubleshooting and safe updates

| Symptom | Check first |
| --- | --- |
| Quest absent | Mod enabled, correct `quests` folder, schema/loader errors and stable unique IDs. |
| Recipe absent | Provider mounted, exact table/row/category, recipe registration and station actor row handle. |
| Scroll crafts but cannot be used | Correct helper/template, native registration, matching item IDs and the supported scripted `UseItem` override. |
| Progress remains zero | Accepted character, active stage, `Acquire` versus `Collect`, exact asset/class paths and authoritative event credit. |
| Board icon blank | Mounted texture object path and UI fallback; not a filesystem image path. |
| Ownership conflict | Duplicate quest/identity or competing mod. Consult [RS-QST-001](../ERROR-CODES.md#rs-qst-001); do not rename IDs as a blind workaround. |
| Source/cache updating | Cached titles/artwork are presentation references, not proof that gameplay registration succeeded. Use [cache bundles](../CACHE-BUNDLES.md). |

Keep folder name, quest key, persistence ID and stage/objective IDs stable.
Presentation changes and gameplay changes have different migration checks;
changing rewards, costs, repeat rules or objective identity is not a cosmetic
update. Back up and test old progress before release. For repeatable quests,
author `Repeatable`, `Repeat` and optional `RepeatReward` deliberately: reset
eligibility does not automatically accept another run.

See [quest reference](../reference/loaders/quests.md),
[scroll authority and validation boundaries](../QUEST-SCROLLS.md),
[troubleshooting](../TROUBLESHOOTING.md) and the
[diagnostic code catalogue](../ERROR-CODES.md).
