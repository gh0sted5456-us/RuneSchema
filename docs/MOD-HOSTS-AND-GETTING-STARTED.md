<section class="rs-intro" aria-labelledby="runeschema-title">
  <span id="ready-to-use-description-for-curseforge-or-nexus-mods"></span>
  <p class="rs-intro__eyebrow">The runtime · The content · The tools</p>
  <h1 id="runeschema-title">RuneSchema</h1>
  <p class="rs-intro__lead">A shared modding framework for RuneScape: Dragonwilds. Build new content, connect it to the game's systems, and keep each add-on together in one manageable place.</p>
  <div class="rs-actions">
    <a class="md-button md-button--primary" href="../INSTALLATION/">Install RuneSchema <span aria-hidden="true">→</span></a>
    <a class="rs-text-link" href="#functions-and-package-reference">Explore the functions ↓</a>
  </div>
</section>

## A foundation for your Dragonwilds mods

RuneSchema gives creators a consistent way to describe content and connect it
to *RuneScape: Dragonwilds*. An add-on can be a small JSON/JSONC adjustment to
an existing item, a new cooked Unreal package, or a larger collection of
equipment, crafting, characters and stories. The same framework brings those
pieces together, so an author can concentrate on the mod rather than rebuild
its loading and integration from scratch.

For players, that means installing RuneSchema once and keeping each add-on in
its own folder. Content definitions, cooked packages and supported Lua
companions travel together. The mod list controls which add-ons are enabled
and their loading order; a simple adjustment does not need a large package,
and a substantial expansion does not need to scatter its files across the
installation.

<div class="rs-capabilities">
  <article class="rs-capability">
    <span class="rs-capability__index">01 / CONTENT</span>
    <h3>Items with a place in the world</h3>
    <p>Connect equipment, appearances, recipes, crafting stations and buildable pieces. Use schemas, cooked content, or both.</p>
  </article>
  <article class="rs-capability">
    <span class="rs-capability__index">02 / WORLDS</span>
    <h3>More than an asset collection</h3>
    <p>Describe NPCs, dialogue, quests, encounters, vendors, resources, journals and lore through supported loader contracts.</p>
  </article>
  <article class="rs-capability">
    <span class="rs-capability__index">03 / WORKSPACE</span>
    <h3>Tools alongside the runtime</h3>
    <p>Use Helpy to browse content, inspect data, author supported definitions and review runtime diagnostics.</p>
  </article>
</div>

## From definitions to working content

RuneSchema reads enabled mod folders, mounts their cooked packages and
registers supported definitions with the native systems that use them. Its
loaders cover items and equipment, crafting and processing, world actors,
story content, player rules and presentation. Authors can override supported
properties without replacing an entire cooked package, while cooked
registry declarations can announce content through the same framework.

This is an integration layer, not a replacement for Unreal's asset creation
or cooking tools. New meshes, sounds and other game assets still need a
compatible cook. Likewise, mounting a PAK does not by itself confirm that its
items, recipes or quests are registered and usable. RuneSchema validates the
contracts it knows and reports unresolved requirements instead of treating a
successful mount as proof that everything worked.

Stable identities matter beyond the first launch. RuneSchema tracks supported
content ownership and gates cleanup against verified live registration.
That helps distinguish a genuinely orphaned entry from content that simply
has not loaded. Backups and safe update/removal practices remain important;
the framework does not make arbitrary mod-list changes risk-free.

## Helpy: the companion workspace

Helpy puts supported runtime tools in a separate desktop workspace. Players → Items and the
detachable Item Bay make content easier to browse and inspect. Crafting,
transmog, map and authoring workspaces bring related actions together, with
source-aware icon caches to avoid repeatedly requesting the same pictures.

Diagnostics combines grouped issues and searchable live logs with linked
codes. Helpy also has a server-local configuration workspace. Server edits
are staged for offline application; they are not a remote administration
endpoint or permission bypass. Individual actions still depend on native
readiness, world collision, player context and server authority.

The same principle applies to multiplayer: clients and servers need compatible
content, and a visual badge is not authorization. A dedicated server running
in an interactive Windows session can use the desktop workspace; a service
with no desktop cannot display a window. Client-only visuals may need an
imported cache, without changing which item definitions the server actually
has registered.

## One framework, deliberately packaged

RuneSchema runs on a compatible, unmodified UE4SS installation. It does not
ship a patched replacement `UE4SS.dll`, promise compatibility with every future
host build, or load native DLL companions from an add-on's `/ue4ss` folder.
Supported Lua packages and cooked Blueprint companions have their own defined
layouts and lifecycle rules.

Players can follow the [installation guide](INSTALLATION.md). Creators can
start with [one small mod](AUTHORING-GUIDE.md), then choose the loaders their
idea needs. Hosting-service reviewers and mod authors can link this page to
explain the dependency and package contents without redistributing the
framework inside every add-on.

!!! note "Download and source version"
    Download the build identified by the [current release page](CURRENT-RELEASE.md).
    The published ZIP is **0.8.0.1**; **0.8.0.1b** is the newer source/tag update.
    Read the [release notes](RELEASE-0.8.0.1.md) and [source changes](CHANGELOG-0.8.0.1b.md).
    Automated checks are not a guarantee
    that every authored feature has passed live-world or multiplayer testing.

<div class="rs-functions-divider" markdown>

## Functions and package reference

The practical reference begins here: what each part does, where it belongs,
and how to put a small add-on together. Include only what your mod needs.

</div>

### What players install

The **RuneSchema framework** is the dependency installed at
`ue4ss/Mods/RuneSchema/`. The [current release](CURRENT-RELEASE.md) ships one
ZIP containing its native DLL, Lua helpers, settings, optional
plugins, cooked bridge packages, and consolidated license notices.
Mapping data is generated/reused in the platform AppData cache, not bundled.

A **RuneSchema add-on** normally installs as one folder beneath
`ue4ss/Mods/RuneSchema/mods/<ModName>/`. It may contain only JSON/JSONC, or it
may combine data definitions with its own cooked PAK and Lua companion. An
add-on does **not** need to repackage RuneSchema's DLL or shared bridges. Its
author should name RuneSchema and any other required mod as dependencies.

If you are installing an add-on, install the framework once, then put the
add-on's complete folder under `RuneSchema/mods/`. Do not scatter its files
across the game's PAK folder or the top-level UE4SS `Mods` folder.

```text
ue4ss/Mods/
  RuneSchema/
    dlls/main.dll                 # framework, not an add-on file
    scripts/main.lua              # framework startup helper
    plugins/                      # bundled optional bridges and Helpy
    settings/settings.ini         # legacy settings.jsonc auto-migrates
    mods/
      runeschema.txt              # enabled add-ons and their order
      MyMod/
        assets/10-items.jsonc     # include only folders the mod needs
        recipes/20-recipes.jsonc
        paks/MyPackage/           # optional cooked Unreal content
          MyPackage.pak
          MyPackage.utoc
          MyPackage.ucas
        logicmods/MyLogic/        # optional ModActor package
        ue4ss/MyLuaPackage/       # optional complete Lua companion
          Scripts/main.lua
          config.json
```

The example shows possible folders, not a requirement to include every one.
Keep a `.pak`, `.utoc`, `.ucas`, and any matching `.sig` together when those
files were produced by the same cook. Do not create empty companion files to
make a package look complete.

### File types and why they appear

| File type | Where it belongs | What it does |
| --- | --- | --- |
| `.zip` | The downloadable framework or add-on archive | Preserves the install layout; it is not itself a runtime file. |
| `.json`, `.jsonc` | An add-on's content folders; framework `settings/`; plugin `plugin.json` files | Data definitions, settings, and plugin descriptions. JSONC permits comments. |
| `.pak` | An add-on's `paks/<Package>/` or `logicmods/<Package>/`; bundled bridge `plugins/*/paks/<Package>/` | Cooked Unreal game assets and data. `logicmods` explicitly opts a package with a ModActor into client-side Blueprint startup; ordinary `paks` does not. |
| `.utoc`, `.ucas` | Beside the matching `.pak` when the cook generated IoStore files | Matching Unreal container index and data; they are parts of the cooked package, not separate mods. |
| `.sig` | Beside a cooked package only when generated by the cooking process | Optional matching Unreal package signature. |
| `.lua` | Framework `scripts/`; optional add-on `ue4ss/<LuaPackage>/Scripts/main.lua` and its helpers | UE4SS startup or explicitly packaged Lua behavior. The complete companion keeps its config and helper files. Lua deserves normal script review. |
| `.dll` | Framework `dlls/main.dll`; bundled native plugin such as `plugins/RuneSchema.Helpy/dll/` | Native UE4SS framework or plugin code. It deserves normal binary review. A DLL inside an add-on's `ue4ss` folder is not a supported RuneSchema companion. |
| `.usmap` | `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/cache/mappings/` | Generated Unreal reflection data, not an executable; no longer bundled under `dlls`. |
| `.ps1` | The bundled Helpy assets, for the explicitly run offline server-settings helper | Applies reviewed staged scalar settings only while the server is stopped. It is executable administrative code, not an add-on loader or an automatic startup script. |
| `.txt` | `enabled.txt`, `plugins.txt`, `mods/runeschema.txt`, mapping notes, and `settings/licenses/` | Enable lists, documentation, and consolidated legal notices. RuneSchema can also write LogicMod handoff lists to `settings/` at runtime. |
| `.md` | READMEs and installation or authoring guidance | Human-readable documentation; not game code. |

Game saves (`.sav`), `UE4SS.log`, crash dumps (`.dmp`), generated handoff
lists, editor-only loose `.uasset` files, installers, and build scripts are not
normal RuneSchema add-on payloads. A source repository may contain build tools
that do not appear in the player-facing release ZIP.

For reviewers: the path matters as much as the extension. A cooked PAK under
`paks` is different from a ModActor package under `logicmods`; a bundled
framework DLL is different from an unexpected DLL inside an add-on. These
descriptions explain legitimate roles, **not** a request to exempt Lua, DLLs,
or PAKs from security scanning. Please review executable content as code and
keep matching Unreal container files together.

### Loader functions: what can an add-on change? {#what-can-an-add-on-change}

RuneSchema has 22 data-loader folders. An author uses only the relevant ones:

| Folders | Typical work |
| --- | --- |
| `assets`, `equipment`, `effects` | Items, equipment behavior, and supported effects. |
| `recipes`, `buildings`, `journal` | Crafting or processing recipes, buildable stations and pieces, and journal entries. |
| `quests`, `dialogue`, `events`, `lore` | Story, choices, encounters, and readable lore. |
| `npc`, `spawns`, `vendors` | World characters, actors or resources, and shops. |
| `players`, `nameplates`, `courses` | Supported player rules, world names, and course records. |
| `blueprints`, `niagara`, `strings` | Supported Blueprint or interface adjustments, visual effects, and text replacements. |
| `raw`, `registry`, `enums` | Supported game-table rows, bridge declarations, and named values. |

See the [loader folder guide](LOADER-WALKTHROUGHS.md) for individual details.
The `paks`, `logicmods`, and `ue4ss` folders are optional companion-content
folders, not three more JSON loaders.

### Runtime and workspace functions

| Function | What it does | Reference |
| --- | --- | --- |
| Enable list and load order | Keeps add-ons independently manageable and places dependencies before consumers | [Manage mods](users/manage-mods.md) |
| Cooked mounting and declarations | Mounts matching package containers and connects supported registry metadata | [PAK content](PAK-CONTENT-GUIDE.md) |
| Recipe integration | Connects station, ingredients, outputs and supported discovery policies | [Stations and recipes](STATIONS-AND-RECIPES.md) |
| Identity and save maintenance | Validates supported ownership/identity records before cleanup; preserves unresolved content when certification is incomplete | [Save safety](SAFE-SAVE-AND-LEDGER.md) |
| Lua companions | Loads supported packaged Lua behavior without changing UE4SS.dll | [Companion folders](PSEUDO-LOADERS.md) |
| Helpy authoring | Inspects supported live data and exports reviewed definitions and reports | [Authoring tools](ADVANCED-AUTHORING-TOOLS.md) |
| Icon caching | Uses source-scoped cached thumbnails, with import/export and bounded capture | [Icon cache](HELPY-ICON-CACHE.md) |
| Diagnostics | Groups issues, searches recent live logs and exports reviewed local evidence | [Troubleshooting](TROUBLESHOOTING.md#helpy-live-log-and-help-codes) |
| Server-local settings | Stages documented server settings, then applies them only with the server stopped | [Dedicated server reference](servers/dedicated-setup.md#verified-settings-and-observed-fields) |

### Make your first RuneSchema mod

For a complete line-by-line lesson, use [FModel export to mod](modders/fmodel-to-mod.md)
and the [Increased Storage chest example](modders/bigger-chests.md). Continue
with [creature drops](modders/creature-drops.md), [cooked-item patches](modders/cooked-item-patches.md)
or [clones](modders/item-clones.md) once that first change works.

Start with one visible change to existing content. The included
[Golden Interaction Text example](examples/GoldenInteractionText/README.md)
shows a small color edit with no new cooked assets. Once that works, add a
recipe or item, and only then add a cooked PAK if your idea needs new game
assets.

1. Install the supported UE4SS setup for your game store, then install the
   [RuneSchema framework](INSTALLATION.md). Start the game once and confirm the
   RuneSchema version appears in `UE4SS.log`.
2. Create `RuneSchema/mods/MyMod/`. Add only the loader folder needed for your
   first change—for example, `strings/` for a supported text replacement or
   `raw/` for an existing DataTable row.
3. Add `MyMod : 1` to `RuneSchema/mods/runeschema.txt`. Its line position sets
   its order relative to other add-ons. Use `MyMod : 0` to disable the entire
   mod on the next game launch.
4. Use [Advanced Authoring & Tools](ADVANCED-AUTHORING-TOOLS.md) to search a
   loaded record, choose a supported starter format, select fields, build a
   preview, check dependencies, and export JSONC. Or start from a matching
   [example](EXAMPLES.md). Do not guess object paths or save identities.
5. Put the completed file in the matching folder under `MyMod/`. Keep released
   object paths, internal names, and persistence IDs stable. Add a PAK only
   when you need new cooked assets such as a mesh, sound, item, or station.
6. Restart the game. Read the RuneSchema summary in `UE4SS.log`, then test on
   a backed-up character: first world entry, return to menu, re-entry, and a
   full game restart. Test host and client separately for multiplayer content.
7. Package the **whole `MyMod` folder** and document its RuneSchema version,
   dependencies, installation path, and safe update/removal steps. Do not
   include a second copy of UE4SS or RuneSchema unless the upload is the
   framework itself.

If your mod adds new cooked content, create the assets in Unreal and cook the
package first. Put its matching container files under `paks/<PackageName>/`,
then use RuneSchema's supported folders to connect that content where needed.
For example, a new weapon can have its mesh and item in the PAK, an `assets`
definition for its item path, and a `recipes` definition to make it craftable.
Mounting a PAK is only the first step: confirm the item and recipe appear in
the live game, not just in the log as mounted files.

The [first-mod guide](AUTHORING-GUIDE.md) continues from here. For new Unreal
content, follow the [PAK guide](PAK-CONTENT-GUIDE.md); for cooked ModActors or
Lua, read the [companion-folder guide](PSEUDO-LOADERS.md). Before removing a
mod from an existing character, read [save safety](SAFE-SAVE-AND-LEDGER.md).

### Suggested description for an add-on listing

> Requires RuneSchema and its supported UE4SS setup. Install this archive's
> `<ModName>` folder under `ue4ss/Mods/RuneSchema/mods/`, enable it in
> `RuneSchema/mods/runeschema.txt`, and restart the game. Keep all included
> cooked PAK container files together. Back up a character before changing
> gameplay mods; see the RuneSchema installation and save-safety guides.

This page is a description of RuneSchema's package format, not a statement of
CurseForge or Nexus Mods policy. Each hosting service retains its own review
and security rules.
