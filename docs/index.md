---
hide:
  - toc
---

<div class="rs-hero" markdown>
<h1 class="rs-sr-only">RuneSchema</h1>

<img class="rs-hero__banner" src="assets/images/runeschema-banner.png" alt="RuneSchema">

<div class="rs-hero__eyebrow">
<span class="rs-version">RuneSchema 0.7.7.9m</span>
<span class="rs-platform rs-platform--steam">Steam / GOG</span>
<span class="rs-platform rs-platform--gamepass">Game Pass / WinGDK</span>
</div>

**Mods that load together, with a clear path from content to the game.**

RuneSchema loads enabled JSON/JSONC mods and their cooked PAKs for *RuneScape:
Dragonwilds*. It connects supported content to live game systems and checks
saved identities against what actually loaded.

<div class="rs-actions" markdown>
[Download](CURRENT-RELEASE.md){ .md-button .md-button--primary }
[Explore the guides](#choose-your-path){ .md-button }
</div>
</div>

## Choose your path

<div class="grid cards rs-card-grid" markdown>

-   :material-account:{ .lg .middle } **Users**

    Install one package, enable or disable mods, keep saves safe, and solve
    startup or world-entry problems.

    [:octicons-arrow-right-24: User guide](users/index.md)

-   :material-hammer-wrench:{ .lg .middle } **Modders**

    Start with a working loader file or cook new content, then connect items,
    recipes, stations, attacks, dialogue, and more.

    [:octicons-arrow-right-24: Modder guide](modders/index.md)

-   :material-server:{ .lg .middle } **Servers**

    Install the same gameplay content, understand which side owns each action,
    and protect world saves during mod-list changes.

    [:octicons-arrow-right-24: Server guide](servers/index.md)

</div>

## Find your next step

| If you want to… | Go to… |
| --- | --- |
| Install or update RuneSchema | [Installation](INSTALLATION.md) |
| Turn a mod off without loading its PAK or companion | [Manage mods and load order](users/manage-mods.md) |
| Understand cleanup before removing a mod | [Save safety](SAFE-SAVE-AND-LEDGER.md) |
| Make a new cooked item show up | [First cooked item walkthrough](modders/first-cooked-item.md) |
| Connect a station to recipes | [Stations and recipes](STATIONS-AND-RECIPES.md) |
| Register a cooked attack or spell | [Weapons and items](WEAPONS-AND-ITEMS.md) and [registry declarations](COOKED-PAK-REGISTRY-MANIFEST.md) |
| Set up a dedicated server | [Server setup](servers/dedicated-setup.md) |
| Find one loader's exact fields | [Loader reference](LOADER-WALKTHROUGHS.md) and [schemas](SCHEMAS.md) |

## What RuneSchema does—and does not do

An enabled mod may contribute loader definitions, PAKs, a client LogicMod,
and an optional Lua companion. The game still needs valid cooked assets and
the right native relationships. **Mounted** does not mean **registered**, and
**registered** does not mean an attack, station, or interface is fully usable.

RuneSchema's save cleanup checks applicable live registries after enabled
content loads. It does not remove a saved ID merely because it came from a
mod. [Read the save boundaries](SAFE-SAVE-AND-LEDGER.md) before changing a
gameplay mod list.

For every page on the site, use the [documentation map](DOCUMENTATION-INDEX.md).

## Need a hand?

Search a specific question with [Ask RuneSchema](ASK-RUNESCHEMA.md), or use
the [troubleshooting guide](TROUBLESHOOTING.md) to follow a problem from its
first failed loading step.
