# Recipe discovery without separate raw patches

RuneSchema 0.8.0.0 accepts a `Discovery` block directly in each `/recipes` body,
including overrides keyed by a full cooked recipe object path. It compiles the
condition into the game's native `ProgressionBundleRow.UnlockQuery`; it does
not add a per-frame polling rule engine or force-write character unlock sets.

## Discover the dye and equipment

```jsonc
{
  "/Example/Recipes/RECIPE_BlackArmor.RECIPE_BlackArmor": {
    "Unlock": false,
    "Discovery": {
      "AllItems": [
        "/Example/Items/ITEM_BlackDye.ITEM_BlackDye",
        "/Example/Items/ITEM_Armor.ITEM_Armor"
      ]
    }
  }
}
```

All paths above are placeholders. Replace them with existing cooked object
paths. These are native **first-pickup history** prerequisites, not requirements
to keep the items in inventory or consume them. Either pickup order satisfies
the same query. A trim recipe can add its matching leaf to the `AllItems` list.
The game's normal native progression pass evaluates the registered query;
existing-character backfill, reconnect and multiplayer delivery still require
in-game validation of the integration.

The recipe retains its cooked identity, ingredients, outputs and station link.
Use `AddTo` separately if the recipe does not already have its station placement.
Changing discovery never revokes progress that the character already learned.

## Supported native conditions

| Condition | Native fact checked |
| --- | --- |
| `AllItems: [paths]` | Every listed item has been picked up. |
| `AnyItems: [paths]` | At least one listed item has been picked up. |
| `AllActors: [classPaths]` | Every listed actor class has been interacted with. |
| `AnyActors: [classPaths]` | At least one listed actor class has been interacted with. |
| `SkillLevel: {"Skill": "cooked path", "Level": 5}` | The native skill-level prerequisite. |

Actor references must resolve to the class accepted by the native operand.
Interacting with a station is not the same event as constructing that station.

A native query supports one or two operands. Use `All` for AND or `Any` for OR:

```jsonc
"Discovery": {
  "All": [
    {"AllItems": ["/Example/Items/ITEM_BlackDye.ITEM_BlackDye"]},
    {"AnyActors": ["/Example/Stations/BP_DyeStation.BP_DyeStation_C"]}
  ]
}
```

Lists accept 1..64 unique references. Groups accept 1..2 operands. Nested
groups, `LearnedRecipes`, arbitrary callbacks and guessed native enum values
are rejected rather than weakened to an unconditional unlock. In particular,
`dye discovered AND (base recipe learned OR base equipment obtained)` is **not**
implemented by this pickup-only example. It needs a separately verified
learned-recipe capability before it can be authored as that exact rule.

## PAK defaults and overrides

- Leave `Discovery` absent, or use `"Discovery": "Native"`, to preserve cooked
  progression. `Unlock: false` does not synthesize a discovery rule.
- Set `Unlock: true` **without** a `Discovery` block for automatic grants.
  Supplying both policies is rejected to avoid bypassing the condition.
- Put a `Discovery` block in `LoaderDefinitions.Recipes` inside the cooked
  registry asset to ship a mod's default rule with its PAK. The body is the
  same as the loose `/recipes` body. No separate `/raw` file is needed.
- A loose definition for the same cooked recipe path overrides that in-PAK
  loader body. For example, a loose body containing only `Unlock: true`
  replaces the PAK's loader-provided conditional policy with automatic grants.
- Mod-owned progression tables must actually be part of the component's
  active progression table. Merely mounting an unrelated DataTable does not
  cause native rules in that table to be evaluated.

The adapter discovers the active table from reflected ProgressComponent data,
with the conventional native table as a startup fallback. It requires the
native row/query layout, validates references through native property classes,
and stages the complete batch before mutation. It detaches **only the target
recipe** from old progression bundles, preserving their other recipes,
buildings, achievements, quest changes and conditions; then it adds one
stable RuneSchema row for the new policy. Shared bundles are never wholesale
replaced. A conflict or validation failure leaves the rejected transaction
uncommitted; commit readback failures roll back affected rows. Known enrollment
failures defer automatic character cleanup for that session.

This governs automatic native progression-bundle links. Explicit gameplay
grants from quests, consumables, shops or another mod's code remain explicit
grants; it is not a security permission that intercepts every native unlock.
The recipe's other native flags remain as authored.

**Fully restart after changing discovery.** Live reload is rejected because
existing ProgressComponents may already cache their progression data. A PAK
must be recooked to change its embedded default, or use a loose override.
Large per-item catalogues need native startup/event performance testing;
passing parser/build tests is not proof of game-side indexing or live delivery.

See [the schema](schemas/recipe-discovery.schema.json),
[recipes](reference/loaders/recipes.md) and
[cooked PAK declarations](COOKED-PAK-REGISTRY-MANIFEST.md).
