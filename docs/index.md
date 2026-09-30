---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.7.0</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**JSON/JSONC mod authoring and runtime support for RuneScape: Dragonwilds.**

One runtime. One content definition. Storefront and multiplayer handling are
selected automatically at runtime.

<div class="rs-actions" markdown>
[Start authoring](AUTHORING-GUIDE.md){ .md-button .md-button--primary }
[Loader reference](LOADER-WALKTHROUGHS.md){ .md-button }
[Examples](EXAMPLES.md){ .md-button }
[Compatibility](COMPATIBILITY-BACKBONE.md){ .md-button }
</div>
</div>

## Start here

<div class="grid cards rs-card-grid" markdown>

-   :material-hammer-wrench:{ .lg .middle } **Authoring guide**

    Mod layout, load order, cooked assets, saves, multiplayer, and testing.

    [:octicons-arrow-right-24: Open guide](AUTHORING-GUIDE.md)

-   :material-folder-cog:{ .lg .middle } **Loader reference**

    Pick the loader for the job, then open its focused reference page.

    [:octicons-arrow-right-24: View loaders](LOADER-WALKTHROUGHS.md)

-   :material-cube-outline:{ .lg .middle } **Unreal + RuneSchema**

    Connect cooked Unreal content to recipes, quests, dialogue, stations, and items.

    [:octicons-arrow-right-24: Unreal workflow](unreal-runeschema/index.md)

-   :material-layers-triple:{ .lg .middle } **Compatibility**

    Steam/GOG, Game Pass/WinGDK, multiplayer, mappings, and saves.

    [:octicons-arrow-right-24: Compatibility](COMPATIBILITY-BACKBONE.md)

</div>

## Safety and recovery

- [Safe Clean and save boundaries](SAFE-SAVE-AND-LEDGER.md)
- [Manual save recovery](MANUAL-SAVE-RECOVERY.md)
- [Current release](CURRENT-RELEASE.md)

<div class="rs-advanced-only" markdown>

## Developers

- [Developer guide](DEVELOPER-GUIDE.md)
- [Plugin API](API-REFERENCE.md)
- [JSON schemas](SCHEMAS.md)

</div>

!!! note
    Historical development notes stay in Git history. The website documents current supported behavior.

!!! warning
    Live Unreal reflection is authoritative. A valid JSON shape does not guarantee that an object or field still exists after a game update.
