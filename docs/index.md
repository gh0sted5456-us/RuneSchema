---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">Current Release</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**JSON/JSONC mod authoring and runtime support for RuneScape: Dragonwilds.**

<div class="rs-actions" markdown>
[Start authoring](AUTHORING-GUIDE.md){ .md-button .md-button--primary }
[Loader reference](LOADER-WALKTHROUGHS.md){ .md-button }
[Ask RuneSchema](ASK-RUNESCHEMA.md){ .md-button }
[Examples](EXAMPLES.md){ .md-button }
</div>
</div>

## For mod authors

<div class="grid cards rs-card-grid" markdown>

-   :material-hammer-wrench:{ .lg .middle } **Authoring guide**

    ---

    Mod layout, load order, cooked assets, mappings, multiplayer, logs, and testing.

    [:octicons-arrow-right-24: Open guide](AUTHORING-GUIDE.md)

-   :material-folder-cog:{ .lg .middle } **Loaders**

    ---

    Pick the right loader and see its rules, examples, and FAQ.

    [:octicons-arrow-right-24: Loader reference](LOADER-WALKTHROUGHS.md)

-   :material-account-edit:{ .lg .middle } **Character creation**

    ---

    Add or patch character options, hair, facial hair, and native menu data.

    [:octicons-arrow-right-24: Character creation](CHARACTER-CREATION-AUTHORING.md)

-   :material-home-edit-outline:{ .lg .middle } **Buildings**

    ---

    Clone building entries, use cooked replacement actors, and place them in the build menu.

    [:octicons-arrow-right-24: Building cloning](BUILDING-CLONING.md)

-   :material-cube-outline:{ .lg .middle } **Unreal + RuneSchema**

    ---

    Connect cooked Unreal content to recipes, quests, dialogue, stations, and RuneSchema registration.

    [:octicons-arrow-right-24: Unreal + RuneSchema](unreal-runeschema/index.md)

-   :material-layers-triple:{ .lg .middle } **Compatibility**

    ---

    Steam/GOG, Game Pass/WinGDK, mappings, plugins, multiplayer, and save boundaries.

    [:octicons-arrow-right-24: Compatibility](COMPATIBILITY-BACKBONE.md)

</div>

## Save safety and recovery

<div class="grid cards rs-card-grid rs-card-grid--compact" markdown>

-   :material-shield-check:{ .lg .middle } **SafeSave & ownership**

    ---

    How RuneSchema tracks owned content and retires missing mod identities.

    [:octicons-arrow-right-24: SafeSave](SAFE-SAVE-AND-LEDGER.md)

-   :material-backup-restore:{ .lg .middle } **Manual recovery**

    ---

    Backup-first recovery steps for supported save problems.

    [:octicons-arrow-right-24: Recovery guide](MANUAL-SAVE-RECOVERY.md)

</div>

<div class="rs-advanced-only" markdown>

## Advanced

<div class="grid cards rs-card-grid rs-card-grid--reference" markdown>

-   :material-code-braces:{ .lg .middle } **Developer guide**

    ---

    Runtime architecture, storefront lanes, saves, build flow, Helpy, and performance.

    [:octicons-arrow-right-24: Developer guide](DEVELOPER-GUIDE.md)

-   :material-api:{ .lg .middle } **Plugin API**

    ---

    Native plugin ABI, lifecycle callbacks, host functions, and services.

    [:octicons-arrow-right-24: API reference](API-REFERENCE.md)

-   :material-code-json:{ .lg .middle } **JSON schemas**

    ---

    Machine-readable schemas for strict authoring surfaces.

    [:octicons-arrow-right-24: Schema reference](SCHEMAS.md)

-   :material-history:{ .lg .middle } **Current release**

    ---

    Public release notes for the current Nexus release.

    [:octicons-arrow-right-24: Release notes](CURRENT-RELEASE.md)

</div>

</div>

!!! info "Current release"
    The website documents the current supported RuneSchema release. Historical development checkpoints stay in Git history.

!!! warning "Runtime reflection decides compatibility"
    Examples show supported authoring shapes. Unreal reflection still determines whether a target object, row, field, and value type are valid at runtime.
