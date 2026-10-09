---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.7.6m</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**JSON/JSONC mod authoring and runtime support for RuneScape: Dragonwilds.**

RuneSchema helps Dragonwilds mods load together, connect new PAK content to the
game, and remove missing saved references safely when a mod is uninstalled.

<div class="rs-actions" markdown>
[Read the Monday update](CURRENT-RELEASE.md){ .md-button .md-button--primary }
[Create PAK content](PAK-CONTENT-GUIDE.md){ .md-button }
[Start a mod](AUTHORING-GUIDE.md){ .md-button }
[Blueprint and Lua companions](PSEUDO-LOADERS.md){ .md-button }
[Get help](TROUBLESHOOTING.md){ .md-button }
</div>
</div>

## Start here

<div class="grid cards rs-card-grid" markdown>

-   :material-hammer-wrench:{ .lg .middle } **Create your first mod**

    Learn the folder layout, load order, safe testing steps, and how to share a mod.

    [:octicons-arrow-right-24: Open guide](AUTHORING-GUIDE.md)

-   :material-package-variant-closed:{ .lg .middle } **Create new PAK content**

    Package new weapons, items, icons, meshes, stations, and other Unreal content.

    [:octicons-arrow-right-24: Open the PAK guide](PAK-CONTENT-GUIDE.md)

-   :material-sword-cross:{ .lg .middle } **Weapons, stations, and recipes**

    Connect new content to crafting menus and make sure the game recognizes it.

    [:octicons-arrow-right-24: Build a connected mod](WEAPONS-AND-ITEMS.md)

-   :material-shield-check:{ .lg .middle } **Safe updates and removal**

    Understand save cleanup, disabled mods, Steam, Game Pass, and multiplayer.

    [:octicons-arrow-right-24: Read the safety guide](SAFE-SAVE-AND-LEDGER.md)

</div>

## Safety and recovery

- [Safe Clean and save boundaries](SAFE-SAVE-AND-LEDGER.md)
- [Install or update RuneSchema](INSTALLATION.md)
- [Current release and Monday update](CURRENT-RELEASE.md)
- [Troubleshooting](TROUBLESHOOTING.md)

!!! note
    This website describes the supported RuneSchema release. Older experiments
    and internal development notes are kept in Git history instead of the public guide.

!!! warning
    A game update can move or rename content. Back up a test character and check
    the RuneSchema log before opening an important world after any game update.
