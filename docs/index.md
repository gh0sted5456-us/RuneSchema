---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.7.7m</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**JSON/JSONC mod authoring and runtime support for RuneScape: Dragonwilds.**

RuneSchema helps Dragonwilds mods load together, connect new PAK content to the
game, and remove missing saved references safely when a mod is uninstalled.

<div class="rs-actions" markdown>
[See what is new in 0.7.7.7](CURRENT-RELEASE.md){ .md-button .md-button--primary }
[For CurseForge and Nexus](MOD-HOSTS-AND-GETTING-STARTED.md){ .md-button }
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

-   :material-file-document-edit:{ .lg .middle } **Use the in-game authoring tools**

    Search loaded records, build a JSONC starter, check dependencies, and export.

    [:octicons-arrow-right-24: Open the workflow](ADVANCED-AUTHORING-TOOLS.md)

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

## What the current release supports

- Load RuneSchema JSON/JSONC definitions and mod-owned cooked PAKs in the
  order set by `runeschema.txt`; `0` keeps a mod and its companions off.
- Connect supported items, recipes, stations, quests, journal entries, NPCs,
  and presentation content to the live game systems that use them.
- Keep an optional cooked `ModActor` and Lua companion inside the same
  RuneSchema mod, with dedicated servers skipping client visual actor startup.
- Use the shared registry and networking bridges shipped in the single ZIP
  when a mod declares a need for those paths.
- Prepare JSONC starters with the in-game authoring workflow, and check saved
  identities against live registries after enabled content loads.

The release includes these paths; it does not guarantee that every independently
cooked PAK or custom Blueprint is valid. See [current release limits](CURRENT-RELEASE.md#what-runeschema-does-not-do)
and [platform guidance](COMPATIBILITY.md) before distributing a gameplay mod.

## Safety and recovery

- [Safe Clean and save boundaries](SAFE-SAVE-AND-LEDGER.md)
- [Install or update RuneSchema](INSTALLATION.md)
- [Current release and downloads](CURRENT-RELEASE.md)
- [Troubleshooting](TROUBLESHOOTING.md)

!!! note
    This website describes the supported RuneSchema release. Older experiments
    and internal development notes are kept in Git history instead of the public guide.

!!! warning
    A game update can move or rename content. Back up a test character and check
    the RuneSchema log before opening an important world after any game update.
