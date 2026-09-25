---
hide:
  - toc
---

<div class="rs-hero" markdown>
<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.5.28</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

<img class="rs-hero__logo" src="assets/images/runeschema-logo.png" alt="RuneSchema logo">

# RuneSchema

**Authoring and runtime reference for RuneScape: Dragonwilds.**

RuneSchema loads validated JSON and JSONC definitions, connects authored data to mounted cooked assets, and provides the runtime bridges needed for self-contained content mods.

<div class="rs-actions" markdown>
[Start authoring](AUTHORING-GUIDE.md){ .md-button .md-button--primary }
[Loader reference](LOADER-WALKTHROUGHS.md){ .md-button }
[Working examples](EXAMPLES.md){ .md-button }
</div>
</div>

## Start here

<div class="grid cards rs-card-grid" markdown>

-   :material-hammer-wrench:{ .lg .middle } **Author a mod**

    ---

    Installation, folder structure, load order, cooked assets, mappings, multiplayer behavior, and validation.

    [:octicons-arrow-right-24: Authoring guide](AUTHORING-GUIDE.md)

-   :material-folder-cog:{ .lg .middle } **Choose a loader**

    ---

    Find the correct loader folder, accepted JSON shape, relationships, and representative definitions.

    [:octicons-arrow-right-24: Loader walkthroughs](LOADER-WALKTHROUGHS.md)

-   :material-flask-outline:{ .lg .middle } **Start from an example**

    ---

    Use focused character, registry, building, vendor, NPC, quest, event, and diagnostic examples.

    [:octicons-arrow-right-24: Example library](EXAMPLES.md)

</div>

## Common authoring areas

<div class="grid cards rs-card-grid rs-card-grid--compact" markdown>

-   :material-account-edit:{ .lg .middle } **Character creation**

    Add or patch character-customization rows and connect them to the native menu.

    [:octicons-arrow-right-24: Open guide](CHARACTER-CREATION-AUTHORING.md)

-   :material-database-edit:{ .lg .middle } **Registry & DataTables**

    Use owned-row transactions, strict target resolution, and reflected-property validation.

    [:octicons-arrow-right-24: Registry patching](REGISTRY-PATCHING.md)

-   :material-home-edit-outline:{ .lg .middle } **Buildings & clones**

    Work with cloned pieces, cooked props, build-menu placement, collision, HISM, and persistence.

    [:octicons-arrow-right-24: Building reference](BUILDING-CLONING-FMODEL-AUDIT.md)

-   :material-shield-check:{ .lg .middle } **SafeSave**

    Understand ownership tracking, retirement, cleanup, retry behavior, and storefront-specific save handling.

    [:octicons-arrow-right-24: SafeSave & ownership](SAFE-SAVE-AND-LEDGER.md)

-   :material-api:{ .lg .middle } **Native plugins**

    Use the RuneSchema ABI, host functions, lifecycle callbacks, services, mappings, and capabilities.

    [:octicons-arrow-right-24: Plugin API](API-REFERENCE.md)

-   :material-code-json:{ .lg .middle } **JSON schemas**

    Use the machine-readable schemas for strict authoring surfaces and editor validation.

    [:octicons-arrow-right-24: Schema reference](SCHEMAS.md)

</div>

## Platform support

<div class="rs-platform-grid" markdown>

<div class="rs-platform-card rs-platform-card--steam" markdown>
### :simple-steam: Steam / GOG

Uses the Steam/GOG runtime lane and backed-up JSON save-cleanup path.

[Compatibility backbone →](COMPATIBILITY-BACKBONE.md)
</div>

<div class="rs-platform-card rs-platform-card--gamepass" markdown>
### :material-microsoft-xbox: Game Pass / WinGDK

Uses the isolated WinGDK runtime lane and Xbox Game Save provider path.

[Game Pass save system →](GAMEPASS-SAVE-SYSTEM.md)
</div>

</div>

## Reference

<div class="grid cards rs-card-grid rs-card-grid--reference" markdown>

-   :material-book-open-page-variant:{ .lg .middle } **Project overview**

    Current runtime structure, release package, and project-level notes.

    [:octicons-arrow-right-24: Overview](OVERVIEW.md)

-   :material-layers-triple:{ .lg .middle } **Compatibility & native parity**

    Storefront detection, mappings, signatures, plugin boundaries, and verified native capabilities.

    [:octicons-arrow-right-24: Compatibility](COMPATIBILITY-BACKBONE.md)

-   :material-history:{ .lg .middle } **Release history**

    Chronological implementation records for current and previous 0.7.5 releases.

    [:octicons-arrow-right-24: Release notes](RELEASES.md)

</div>

!!! info "Current documentation is authoritative"
    Use the **Authoring Guide**, **Loader Walkthroughs**, **SafeSave and Ownership**, **Compatibility Backbone**, and **Plugin API** for current behavior. Release notes and audits preserve implementation history.

!!! warning "Runtime reflection wins"
    Schemas and examples document accepted authoring shapes. Unreal reflection and runtime validation remain authoritative for object, row, field, and type compatibility.
