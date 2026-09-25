# RuneSchema

<div class="rs-hero" markdown>
<div class="rs-hero__mark" aria-hidden="true"><span>{</span><strong>R</strong><span>}</span></div>

## RuneSchema authoring and runtime reference

RuneSchema is a UE4SS runtime for self-contained **RuneScape: Dragonwilds** content mods. It loads validated JSON and JSONC definitions, connects them to mounted cooked assets, and provides multiplayer presentation and authority bridges without replacing vanilla game systems.

[Start authoring](AUTHORING-GUIDE.md){ .md-button .md-button--primary }
[Browse loaders](LOADER-WALKTHROUGHS.md){ .md-button }
[View examples](EXAMPLES.md){ .md-button }
</div>

<div class="rs-status-grid" markdown>
<div class="rs-status-card" markdown>
### Current documentation
**RuneSchema 0.7.5.28**

The current guides describe the universal runtime and its isolated Steam/GOG and Game Pass/WinGDK execution lanes.
</div>

<div class="rs-status-card" markdown>
### Authoring format
**JSON / JSONC + cooked assets**

Definitions are organized by loader folder, validated before mutation, and isolated by mod ownership.
</div>

<div class="rs-status-card" markdown>
### Start with
**Authoring Guide → Loader Walkthroughs**

Use the current authoring references first. Release notes and audits record implementation history and do not override newer contracts.
</div>
</div>

## Choose a path

<div class="grid cards" markdown>

-   :material-hammer-wrench:{ .lg .middle } **Build a content mod**

    ---

    Install RuneSchema, create a mod folder, understand load order, add cooked assets, and validate multiplayer behavior.

    [:octicons-arrow-right-24: Authoring guide](AUTHORING-GUIDE.md)

-   :material-folder-cog:{ .lg .middle } **Find the correct loader**

    ---

    Review all loader folders, accepted JSON shapes, relationships, and representative definitions.

    [:octicons-arrow-right-24: Loader walkthroughs](LOADER-WALKTHROUGHS.md)

-   :material-account-edit:{ .lg .middle } **Extend character creation**

    ---

    Add or patch character customization rows and connect them to the native character-creation menu.

    [:octicons-arrow-right-24: Character creation](CHARACTER-CREATION-AUTHORING.md)

-   :material-database-edit:{ .lg .middle } **Patch DataTables safely**

    ---

    Use owned-row transactions, strict target resolution, reflected-property checks, and dependency references.

    [:octicons-arrow-right-24: Registry patching](REGISTRY-PATCHING.md)

-   :material-shield-check:{ .lg .middle } **Understand save safety**

    ---

    Review ownership tracking, removal behavior, retry rules, and storefront-specific cleanup lanes.

    [:octicons-arrow-right-24: SafeSave and ledger](SAFE-SAVE-AND-LEDGER.md)

-   :material-api:{ .lg .middle } **Build a native plugin**

    ---

    Use the RuneSchema plugin ABI, host functions, lifecycle callbacks, services, mappings, and capability registration.

    [:octicons-arrow-right-24: Plugin API](API-REFERENCE.md)

</div>

## Loader map

| Loader | Primary use |
|---|---|
| `assets` | Items, stats, unlock links, DataAssets, and reflected object patches |
| `blueprints` | Supported reflected class defaults |
| `buildings` | BuildingPieceData registration and cloning |
| `courses` | Course definitions and patches |
| `dialogue` | Conversations and actions |
| `effects` | GameplayEffect class aliases |
| `enums` | Loaded enum extensions |
| `equipment` | Wear-triggered effects and utility behavior |
| `events` | Timed waves driven by dialogue |
| `journal` | Journal and recipe entries |
| `lore` | Lore entries and pages |
| `nameplates` | Reusable nameplate definitions |
| `niagara` | Niagara attachment definitions |
| `npc` | Persistent interactable actors |
| `players` | Player rules and presentation |
| `quests` | Per-character quest definitions |
| `raw` | DataTable rows and patches |
| `recipes` | Crafting, processing, and merchant offers |
| `registry` | Multiplayer action and presentation manifest |
| `spawns` | AI, actors, resources, and building props |
| `strings` | Source-text replacement |
| `vendors` | Reusable RuneSchema stores |

[Open the complete loader reference →](LOADER-WALKTHROUGHS.md)

## Documentation rules

!!! note "Current guides are authoritative"
    Use the **Authoring Guide**, **Loader Walkthroughs**, **SafeSave and Ownership Ledger**, **Compatibility Backbone**, and **Plugin API** for current behavior. Release notes and audit documents preserve implementation history.

!!! warning "Runtime reflection wins"
    JSON schemas and examples document accepted authoring shapes. Unreal reflection and runtime validation remain authoritative for object, row, field, and type compatibility.
