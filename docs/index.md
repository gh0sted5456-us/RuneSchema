---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.5.28</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**Authoring and runtime reference for RuneScape: Dragonwilds.**

RuneSchema loads JSON/JSONC definitions, connects them to cooked assets, and
provides the runtime bridges used by self-contained content mods.

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

    Install RuneSchema, create a mod folder, understand load order, add cooked
    assets, and test the result.

    [:octicons-arrow-right-24: Authoring guide](AUTHORING-GUIDE.md)

-   :material-folder-cog:{ .lg .middle } **Choose a loader**

    ---

    Find the correct loader folder, accepted JSON shape, and working examples.

    [:octicons-arrow-right-24: Loader reference](LOADER-WALKTHROUGHS.md)

-   :material-cube-outline:{ .lg .middle } **Unreal + RuneSchema**

    ---

    Combine cooked Unreal actors and data assets with RuneSchema registration,
    recipes, quests, dialogue, and packaging.

    [:octicons-arrow-right-24: Unreal + RuneSchema](unreal-runeschema/index.md)

-   :material-flask-outline:{ .lg .middle } **Start from an example**

    ---

    Browse focused examples for DataTables, UI patches, quests, events, vendors,
    NPCs, character customization, and buildings.

    [:octicons-arrow-right-24: Example library](EXAMPLES.md)

</div>

## Common authoring areas

<div class="grid cards rs-card-grid rs-card-grid--compact" markdown>

-   :material-account-edit:{ .lg .middle } **Character creation**

    Add or patch customization rows and connect them to the native menu.

    [:octicons-arrow-right-24: Character creation](CHARACTER-CREATION-AUTHORING.md)

-   :material-database-edit:{ .lg .middle } **Registry & DataTables**

    Add owned rows or patch supported existing data with runtime validation.

    [:octicons-arrow-right-24: Registry & DataTables](REGISTRY-PATCHING.md)

-   :material-home-edit-outline:{ .lg .middle } **Buildings & clones**

    Register cooked building pieces, clone compatible data, and control build
    menu placement.

    [:octicons-arrow-right-24: Building reference](BUILDING-CLONING-FMODEL-AUDIT.md)

-   :material-shield-check:{ .lg .middle } **SafeSave**

    Track RuneSchema-owned identities and clean retired content without treating
    unresolved vanilla or third-party data as disposable.

    [:octicons-arrow-right-24: SafeSave & ownership](SAFE-SAVE-AND-LEDGER.md)

</div>

## Platform support

<div class="rs-platform-grid" markdown>

<div class="rs-platform-card rs-platform-card--steam" markdown>
### :simple-steam: Steam / GOG

Uses the desktop runtime lane and backed-up loose JSON save path.

[Compatibility →](COMPATIBILITY-BACKBONE.md)
</div>

<div class="rs-platform-card rs-platform-card--gamepass" markdown>
### :material-microsoft-xbox: Game Pass / WinGDK

Uses the isolated WinGDK runtime lane and Xbox Game Save provider path.

[Game Pass saves →](GAMEPASS-SAVE-SYSTEM.md)
</div>

</div>

<div class="rs-advanced-only" markdown>

## Advanced reference

<div class="grid cards rs-card-grid rs-card-grid--reference" markdown>

-   :material-api:{ .lg .middle } **Plugin API**

    Native plugin ABI, services, capabilities, mappings, and validated bindings.

    [:octicons-arrow-right-24: Plugin API](API-REFERENCE.md)

-   :material-code-json:{ .lg .middle } **JSON schemas**

    Machine-readable schemas for strict authoring surfaces.

    [:octicons-arrow-right-24: JSON schemas](SCHEMAS.md)

-   :material-speedometer:{ .lg .middle } **Startup performance**

    Measured loader stages and storefront-specific startup costs.

    [:octicons-arrow-right-24: Performance notes](STARTUP-PERFORMANCE.md)

-   :material-lifebuoy:{ .lg .middle } **Manual recovery**

    Backup-first save recovery when normal owned-content cleanup cannot complete.

    [:octicons-arrow-right-24: Recovery guide](MANUAL-SAVE-RECOVERY.md)

-   :material-cog-outline:{ .lg .middle } **Internals**

    Native hooks, subsystem boundaries, Helpy rendering, and developer notes.

    [:octicons-arrow-right-24: Native hook support](NATIVE-HOOK-PARITY.md)

-   :material-tag-outline:{ .lg .middle } **Current release**

    Public release notes for RuneSchema 0.7.5.28.

    [:octicons-arrow-right-24: Release](RELEASES.md)

</div>

</div>

!!! info "Current guides are authoritative"
    Use the current Authoring, Loader, Compatibility, SafeSave, and Plugin API
    pages for present behavior. Historical implementation files are kept in the
    repository but are not part of the public manual.

!!! warning "Runtime reflection wins"
    Schemas and examples document accepted authoring shapes. Unreal reflection
    and runtime validation remain authoritative for object, row, field, and type
    compatibility.
