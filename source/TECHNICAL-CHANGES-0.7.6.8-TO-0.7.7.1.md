# RuneSchema 0.7.6.8 Community Edition to 0.7.7.1 Experimental

## Complete technical change and bug-fix report

**Document date:** October 1, 2026  
**Published baseline:** RuneSchema Community Edition 0.7.6.8  
**Experimental target:** RuneSchema 0.7.7.1 Universal, commit `5151dc6` on `experimental`  
**Source comparison:** `c031bfa..5151dc6`  
**Comparison size:** 216 commits, 129 changed files, 5,619 insertions, and 2,134 deletions

## 1. Scope and comparison basis

This report describes the effective runtime, authoring, compatibility, build,
and safety differences between the latest Community Edition represented by the
Nexus release history and the current 0.7.7.1 experimental build. It documents
only behavior present in the completed experimental build.

The Nexus page currently has mixed display metadata: its general page version
is shown as `1`, its long description still contains an older 0.7.5.28 label,
and its changelog's newest entry is 0.7.6.8. The repository contains the
matching `RELEASE-0.7.6.8.md`, so 0.7.6.8 is the technical baseline used here.

Primary references:

- [RuneSchema - RSDW Modding Community on Nexus](https://www.nexusmods.com/runescapedragonwilds/mods/570)
- [0.7.6.8 baseline release note](RELEASE-0.7.6.8.md)
- [0.7.7.1 current release note](CURRENT-RELEASE.md)
- [GitHub comparison: c031bfa to 5151dc6](https://github.com/gh0sted5456-us/RuneSchema/compare/c031bfa...5151dc6)
- [Compatibility architecture](COMPATIBILITY-BACKBONE.md)
- [Storefront native lanes](STOREFRONT-LANES.md)

The 0.7.6.8 baseline already included the Community Edition's persistent NPC,
dialogue, quest, vendor, event, multiplayer, cooked-asset, and native quest
hand-in work. In particular, 0.7.6.8 fixed staged retrieval-quest hand-ins by
checking authoritative inventory, respecting authored hand-in areas and
aggregate requirements, reporting carried/required counts, and preserving
idempotent scaling. Those capabilities remain the starting point rather than
being counted as new 0.7.7.1 work.

## 2. Executive summary

0.7.7.1 is primarily a safety, lifecycle, compatibility, and authoring-surface
release. The central architectural changes are:

1. Persistence cleanup is now a mandatory, registry-authoritative subsystem.
   It removes a persistence identity only when the complete applicable live
   registry cannot resolve it. The diagnostic ledger is not an authority.
2. Cleanup is lifecycle-bound and one-shot. Steam/GOG receives a guarded
   startup file pass; all storefronts receive a reflected character-load
   preflight. Menu/world transitions do not repeatedly rescan or rewrite the
   save.
3. Quest/dialogue identities are protected from premature startup deletion.
   Early cleanup handles item/recipe state; quest and journal validation waits
   for late RuneSchema registrations at character hydration.
4. Dialogue no longer creates an empty hidden quest merely because its mod was
   loaded. Native dialogue state initializes lazily on the first real write.
5. The same DLL now selects Steam/GOG or Game Pass/WinGDK native bindings at
   runtime. Steam signatures are never guessed on WinGDK.
6. Runtime Blueprint authoring gained event-scoped function calls, widget
   discovery, activation, transient UMG, and text styling without a global
   polling scan.
7. Character-option layouts expand dynamically to a maximum of eight columns,
   ten rows per column, and eighty visible choices.
8. Runtime item clones, recipes, vendors, and live subsystem registration were
   hardened so persistence IDs round-trip through native registries before
   downstream use.
9. NPC content no longer has an authored `Multiplayer` mode. A single
   definition is used; current network role determines authority and client
   presentation.
10. Startup failures and partial failures are explicitly distinguished.
    Fatal initialization stops RuneSchema and writes a DID-NOT-START reason;
    isolated loader failures are logged as partial/degraded while safe
    capabilities continue.

## 3. Persistence and save-safety redesign

### 3.1 Authority model

The old cleanup path mixed owner knowledge, loader timing, and save repair.
0.7.7.1 separates those responsibilities:

- The live native registries are the authority for whether an identity exists.
- Mod origin is informational. A persistence ID supplied by the base game, a
  mounted PAK, or a RuneSchema loader is equally valid if it resolves.
- The optional ledger records provenance for diagnostics only. Cleanup never
  reads it and RuneSchema does not depend on it to start or prune.
- Unknown IDs are not removed while any required registry is incomplete.
- A registry entry is accepted only when its map key resolves to one live
  object whose own `PersistenceID` round-trips to that exact key.
- Duplicate IDs resolving to different objects make registration incomplete;
  they do not authorize deletion.
- Item and recipe snapshots are published only after successful registration
  and two identical captures, preventing the first partially populated view
  from becoming cleanup authority.

The registry snapshot covers:

- `ItemSubsystem.PersistenceIDToDataMap`;
- `RecipeSubsystem.PersistenceIDToDataMap`;
- `QuestDataSubsystem.PersistenceIDToDataMap`; and
- the journal subsystem's persistence map when available.

### 3.2 Fields covered by mandatory orphan pruning

When the corresponding live registry is complete, 0.7.7.1 validates these
native character-save locations:

| Registry | Save locations validated |
|---|---|
| Items | `GameProgress.Inventory`, `PersonalInventory`, `Loadout`, `Progress.ItemsPickedUp`, and `Progress.MilestoneMaterialsPickedUp` |
| Recipes | `GameProgress.Progress.RecipesUnlocked` and `RecipesNew` |
| Quests/dialogue | `GameProgress.QuestProgress.Quests` and `QuestTracked` |
| Journal/lore | `GameProgress.Journal.UnlockedEntries` and `UnreadEntries` |

Loadout cleanup also follows `PlayerInventoryItemIndex`: if the referenced
inventory slot was removed because its item ID was unresolved, the dependent
loadout entry is removed without renumbering other slots. `MaxSlotIndex` and
unrelated records are preserved.

Malformed native structures do not become evidence that a persistence ID is
orphaned. In the mandatory unresolved-only path, unsupported layouts fail
closed or are retained rather than being broadly normalized.

### 3.3 Steam/GOG startup cleanup

Steam/GOG loose character JSON receives one guarded process-start cleanup pass,
at the same broad lifecycle point used by the old 0.6/early-0.7 cleaner:

1. RuneSchema registers all available content.
2. It captures the live registries twice.
3. It scans only `.json` character files in the native `SaveCharacters`
   directory, with a maximum of 64 files and 64 MiB total input.
4. It accepts UTF-8 and UTF-16LE character documents and only processes native
   gameplay saves containing `GameProgress` and character metadata.
5. It verifies that the file has not changed between read and write.
6. Before modification it writes a unique
   `.runeschema-startup-<timestamp>.bak` copy.
7. It serializes and parses the proposed result before writing, performs the
   replacement, reads it back, and verifies structural equality.
8. If verification fails, it attempts to restore the original bytes and logs
   the exact backup name if automatic restoration cannot complete.

This pass deliberately marks quest and journal registries incomplete. Hidden
dialogue quests and other RuneSchema quest assets register later than the item
and recipe systems; treating the early quest view as complete caused valid
quest/dialogue IDs to be deleted and then re-created on every restart.

### 3.4 Reflected character-load preflight

The cross-storefront path is a filtered UE4SS `ProcessEvent` pre-callback on
Dominion character JSON load functions. It does not scan every UObject and
does not patch executable entry points.

At that boundary RuneSchema:

- calls content registration again;
- identifies the single gameplay JSON string parameter by reflected type and
  content;
- refuses ambiguous or unsupported parameter layouts;
- requires complete applicable registries;
- performs full item, recipe, quest, and journal validation; and
- writes only the verified JSON string that the game is about to hydrate.

The preflight is consumed once per game process. Returning between world and
menu does not run cleanup again. Dragonwilds then follows its normal save path.

Game Pass/WinGDK uses this reflected boundary and never edits Xbox WGS provider
files directly. Steam/GOG may use both the startup loose-file pass and the
reflected boundary; the process-wide guard prevents repeated preflight cleanup.

### 3.5 Logging and observability

Removed IDs remain visible in the log. The new messages distinguish:

- registry not ready, save unchanged;
- resolved ID retained;
- individual orphan removed;
- category totals;
- startup pass complete;
- cleanup already consumed for this process; and
- fatal write/restore problems.

The optional advanced persistence ledger records mod name, persistence ID,
internal name, and source path for launched RuneSchema content. It requires
advanced runtime diagnostics plus `diagnostics.persistenceLedger`; it is not a
cleanup database, restore history, or runtime dependency.

## 4. Quest and dialogue persistence fixes

### 4.1 Lazy hidden-quest initialization

RuneSchema dialogue state still uses Dragonwilds' native quest-variable save
system. There is no `character_runeschema.json` companion save and no second
host/client persistence authority.

The important change is when the hidden per-mod state quest is created:

- Loading a dialogue mod no longer initializes an `Ungiven` quest record.
- An existing native state remains readable.
- Legacy dialogue state is migrated only if that mod actually has saved
  progress.
- A new state quest is initialized and ownership-stamped only immediately
  before the first real dialogue write.
- Completion remains staged: write pending state, perform the grant/action,
  then write complete state. An uncertain grant is not silently marked done.

This fixes the cycle where RuneSchema injected hidden quest records on startup,
the cleaner removed them, and the next launch repeated the same warnings.

### 4.2 Cleanup timing

Quest and dialogue persistence IDs are no longer evaluated from the early
startup snapshot. They are evaluated at character load after RuneSchema quest
definitions and the hidden dialogue state assets have registered. A quest is
removed only if its ID is absent from the complete quest registry.

The tracked quest field is cleared if it points to an unresolved quest. Normal
quest locations and unrelated quest progress are not blanket-reset.

### 4.3 Automatic quest behavior

The experimental build does not auto-accept or auto-complete authored quests
merely because a definition exists. Quest state changes are driven by the
authored start, stage, completion, hand-in, abandonment, and repeat rules. The
0.7.6.8 authoritative inventory/hand-in fixes remain intact.

## 5. Optional baseline recovery

Baseline recovery is separate from mandatory pruning and is disabled by
default.

### 5.1 Built-in appearance baseline

The DLL contains a small validated appearance-only safety profile. It is not a
full character snapshot. When recovery and the appearance category are enabled,
RuneSchema replaces only a missing or invalid customization handle whose target
DataTable and row cannot be resolved. Valid live choices remain unchanged.

A missing `BodyType` is left to Dragonwilds; an existing but invalid custom
`BodyType` can be repaired from the baked male/A profile. Players can then use
the native appearance editor.

### 5.2 External `default.json`

Users may place exactly:

```text
RuneSchema/settings/defaults/default.json
```

RuneSchema reads that one file once during startup recovery. It does not scan or
watch the folder. The file must be either a native gameplay character document
or a valid profile-only appearance document. Missing or invalid external data
falls back only to the baked appearance profile.

Independent category toggles control:

- appearance repair;
- missing item/inventory/loadout records;
- missing quest records and an empty tracked quest; and
- missing progress entries such as pickups and recipe lists.

The merge is additive. Existing live values and newer progress always win.
Persistence-bearing baseline records are admitted only when their IDs resolve
in the same complete live registries used by the pruner. This is not a blanket
save replacement and cannot silently roll progress back.

## 6. Items, assets, recipes, and vendors

### 6.1 Runtime item clones

`/assets` clones now participate in the native item system rather than existing
only as reflected objects:

- clone dependencies are ordered so a runtime parent clone registers before a
  child clone;
- new items require a canonical 22-character `PersistenceID`, a non-empty
  `InternalName`, a unique destination path, and a compatible source type;
- identity is inserted into both persistence and internal-name maps;
- native network IDs and reverse maps are established;
- registration is applied across every live `ItemSubsystem`, covering overlap
  during world transitions;
- an ID collision, incomplete parent, wrong type, or failed round-trip rejects
  the clone instead of allowing recipes or grants to use a half-registered
  object; and
- failed registration clears the clone identity so it cannot masquerade as
  persistent content.

Compatible consumable-pack clones may replace their native drop contents while
retaining the source pack's behavior. Quest consumables use Dragonwilds'
normal native inventory-routing behavior on both storefronts.

### 6.2 Recipe references and placement

Recipe item references are now path-first:

- Full cooked item object paths are the recommended `ItemData` form.
- Legacy persistence-ID references remain accepted only when they resolve
  uniquely through the item route index.
- A recipe referencing a runtime clone is deferred until clone registration is
  complete.
- The loader reads ingredient/output arrays back from the live `RecipeData` and
  refuses placement if entries or exact counts were truncated or lost.
- `Category` targets native labeled recipe categories; `Array` targets direct
  recipe arrays. Supplying both is rejected.
- Exact `DataTable` paths are supported for custom or ambiguous tables.

The former per-entry global UObject scan was replaced by a refreshed route
index, reducing the cost of larger recipe packs.

### 6.3 Unlock persistence semantics

The current rules distinguish three cases:

1. `Unlock: true` automatic availability is written to both
   `RecipesUnlocked` and Dragonwilds' `RecipesUnlockedThatShouldNotPersist`
   exclusion set. Visiting a world does not pump those automatic IDs into the
   save.
2. A real gameplay progression action may persist a RuneSchema recipe once its
   live `RecipeData` has a canonical registered persistence ID.
3. Vendor-generated offer recipes are transient and are never treated as
   durable learned recipes.

Unlocks are applied to all live progress components, not merely the first one,
which fixes world-transition overlap and Game Pass live-instance gaps.

### 6.4 Vendor isolation

Generated vendor recipes now carry deterministic ownership and live-object
leases. Opening a store removes previous RuneSchema vendor-generated recipes
from the transient visible/non-persistent sets before installing the current
store's validated offers. This prevents stock from bleeding between vendors or
category tabs while leaving vanilla and ordinary authored recipes untouched.

## 7. Character customization and MoreHair-style composition

The character customization asset path now has dedicated layout handling:

- multiple mods can append options using stable `DataHandle.RowName` identity;
- `$AppendUnique` avoids duplicate options;
- `$MergeWhere` can update several exact native rows transactionally;
- missing or duplicate selectors reject the group instead of modifying an
  arbitrary row;
- legacy registry-envelope documents are translated for compatibility;
- live menu patches replay when the character option screen is constructed; and
- the menu column count grows dynamically from the combined option count.

The final 0.7.7.1 layout limit is **eight columns by ten rows**, for at most
eighty visible options. The layout never shrinks an existing valid column count
and rejects a current layout outside the supported 1–8 range.

This replaces the brittle fixed-row behavior that caused one hair mod's column
adjustment to overwrite or mislay another mod's additions.

Automatic `/players` appearance persistence remains independently controlled by
`persistence.characterCustomization` and defaults to off. DataTable rows and
menu options still load when automatic appearance persistence is disabled.

## 8. Runtime Blueprint, widget, and text-authoring changes

0.7.7.1 incorporates the safe portions of the 0.8 Blueprint agility test bed
without replacing stable loader behavior.

### 8.1 `$RuntimeWidget` execution

Runtime widget rules now support:

- `$Call` for existing reflected UFunctions with named JSON arguments;
- `$When` to restrict a rule to observed event names;
- `$Once` for one execution per live owner/world lifetime;
- `$Activate` for CommonUI activation/deactivation;
- exact owner-relative widget paths;
- bounded `$Find` scopes for `WidgetTree`, HUD references, and CommonUI
  container lists; and
- per-rule re-entrancy suppression, weak target tracking, and teardown cleanup.

There is intentionally no unrestricted global `FindAllOf(UserWidget)` authoring
primitive. CDOs, archetypes, destroying/loading objects, oversized arrays, and
ambiguous matches fail closed.

### 8.2 `$TextStyle`

`$RuntimeWidget.$TextStyle` can change live UMG text presentation at the
owner's normal runtime events without a polling scan. Supported fields include:

- `Color`/`ColorAndOpacity`;
- partial `Font` updates, including size and letter spacing;
- shadow color and offset;
- minimum desired width;
- automatic wrapping;
- justification; and
- native rich-text default style fields when exposed by the target.

Known Dragonwilds target maps were added for journals, journal details, world
interaction prompts, and related text widgets. Ordinary text widgets can be
restyled as a whole. Per-word coloring requires a genuine rich-text target or a
cooked widget/style solution; RuneSchema does not reinterpret arbitrary plain
text as rich markup.

### 8.3 `$RuntimeUI`

RuneSchema can create a limited transient UMG tree for overlays and lightweight
controls:

- allowed primitives: CanvasPanel, Border, TextBlock, Image, and Button;
- at most 16 top-level authored widgets, 64 total nodes, depth 8, and 32
  children per node;
- exact safe names and duplicate prevention;
- explicit viewport Z order;
- existing binding/call support for created nodes; and
- owner deactivation/destruction and world teardown remove the transient UI.

It does not synthesize Blueprint bytecode or create a new cooked Blueprint
class.

### 8.4 Game Pass native appearance control

The Game Pass Restore Appearance example now reveals the game's existing
`EditAppearanceButtonSBox`, `EditAppearanceButton`, and input-legend widgets in
the two observed character-select owner classes. It preserves the native
`IA_UI_EditAppearance` action wiring. It no longer overlays a synthetic
RuneSchema button.

## 9. NPC and multiplayer model

The authored NPC `Multiplayer` property is retired:

- new schemas and examples no longer emit it;
- legacy definitions are normalized by discarding the field;
- one NPC definition serves standalone and networked worlds;
- server authority controls gameplay mutations;
- clients receive replicated actors and presentation when the content supports
  merchant, dialogue, lore, or visual interaction; and
- stable actor identity and gameplay fingerprints no longer depend on an
  authored multiplayer toggle.

This removes a class of mismatches where the same installed NPC had different
identity or behavior because two authors chose different mode flags.

## 10. Storefront compatibility

### 10.1 One Universal DLL

0.7.7.1 builds one `main.dll` for Steam/GOG and Game Pass/WinGDK. Startup
detection selects `steam-native` or `gamepass-native`; JSON loaders and plugin
APIs are shared.

Storefront-specific native work remains isolated:

- distinct `UDataTable::Serialize`, memory, name, and object-enumeration paths;
- WinGDK avoids the similar-looking but ABI-incompatible Steam object iterator;
- WinGDK journal persistence uses the validated JournalComponent interface and
  layout checks;
- Steam uses its own AOB contract; and
- a missing or changed native signature disables the dependent feature where
  possible instead of guessing across storefronts.

### 10.2 Save-provider difference

The cleanup decision is shared, but storage is not. Steam/GOG uses loose local
character JSON. Game Pass uses Xbox Game Save containers; RuneSchema never
treats their GUID-named payloads as loose Steam files. Game Pass cleanup is
applied to the character JSON at the game's native hydration boundary.

## 11. Mod discovery, ordering, and plugins

### 11.1 Mod discovery hardening

Only directories containing a recognized RuneSchema loader directory or valid
legacy PAK content are treated as RuneSchema mods. Readmes, backups,
mod-manager bookkeeping, unrelated folders, and unsupported files are ignored.
Case-insensitive loader-directory resolution rejects ambiguous duplicate casing
instead of loading the same logical loader twice.

### 11.2 Authoritative `runeschema.txt`

Explicit `runeschema.txt` rows now load top-to-bottom exactly as written. Prefix
tiers are fallback discovery policy only:

- omitted `AA_` folders remain implicit-first;
- omitted `ZZ_` folders remain implicit-last;
- explicitly listing either prefix makes that exact row authoritative; and
- `0` disables the mod while `1` enables it.

Advanced logging and file auto-reload do not gate initial loader execution.
Disabling RuneSchema in UE4SS `Mods/mods.txt` remains a host-level disable: the
DLL is not started.

### 11.3 Plugin changes

- `RuneSchema.Networking` was renamed to `RSNetworking`.
- Legacy order entries are migrated to the new ID.
- If both old and new plugin folders exist, the new plugin wins and duplicate
  capabilities are avoided.
- Helpy is DLL-only and declares `MountPaks: false`; its obsolete cooked PAK
  was removed.
- PAK mounting is explicit per plugin.
- Non-container files in plugin PAK roots are ignored, while loose
  `.pak/.ucas/.utoc` files outside a named package directory are rejected.
- `plugins.txt` order remains preferred; dependency ordering is applied where
  possible, with deterministic diagnostics for missing dependencies or cycles.
- Plugin failure does not disable RuneSchema core.

## 12. Startup and failure isolation

0.7.7.1 distinguishes fatal initialization from optional capability loss.

Fatal construction/core startup:

- returns no mod instance or aborts all RuneSchema runtime hooks/services;
- stops file watching, runtime jobs, network role monitoring, plugin services,
  and loader ownership;
- logs `[RuneSchema][DID-NOT-START]` with stage and reason; and
- writes `startup-failure.log` containing process ID, stage, and reason.

Partial failure:

- a bad mod section is logged as `[PARTIAL][MOD:...]`;
- a loader finalization problem is logged as `[LOADER:...][PARTIAL]`;
- an unavailable optional service is logged as `[DEGRADED][SERVICE:...]`; and
- successfully initialized, independent capabilities remain active.

The persistence registrar is treated conservatively: if it degrades, RuneSchema
continues other loaders but explicitly states that pruning is unavailable for
that run.

## 13. Build, packaging, mappings, and verification

The build system was substantially hardened:

- version identity is consistently `0.7.7.1` / `RuneSchema 0.7.7.1 Universal`;
- generated CMake/Ninja state moved to a short deterministic `%TEMP%/RSB-*`
  path to avoid Windows path-length failures;
- a configuration fingerprint avoids repeatedly regenerating the large UE4SS
  graph;
- interrupted/incomplete configuration state is detected and rebuilt;
- the pinned UE4SS source checkout and submodules are prepared before CMake;
- private UEPseudo access gets a direct GitHub/Epic authorization diagnostic;
- SSH submodule URLs are rewritten to authenticated HTTPS for Windows Git
  Credential Manager;
- the pinned RSDWArchive USMAP is downloaded by the BAT/PowerShell builder,
  bounded by size, SHA-256 verified, cached, and recorded in
  `build/mappings.lock.json`;
- built DLL imports are audited against the pinned UE4SS host ABI;
- absent code-signing credentials no longer fail packaging;
- Core and Universal ZIPs are produced separately; and
- UE4SS storefront runtimes remain separate distribution assets rather than
  being silently bundled into RuneSchema packages.

The final local verification for commit `5151dc6` compiled the DLL and passed
all 46 enabled contract tests. The output packages were:

```text
dist/RuneSchema-0.7.7.1-Universal.zip
dist/RuneSchema-0.7.7.1-Core.zip
dist/RuneSchema-0.7.7.1.dll
```

New or expanded contracts cover registry-only cleanup, owned-save cleanup,
appearance defaults, character-entry recovery, persistence diagnostics,
storefront selection, native binding resolution, loader-folder casing,
load-order authority, plugin compatibility, asset patching, runtime widgets,
recipe references/placement, world re-entry, and startup configuration.

## 14. Configuration delta

The important current defaults are:

```jsonc
{
  "advancedLogging": false,
  "advancedRuntime": false,
  "diagnostics": {
    "persistenceLedger": false
  },
  "persistence": {
    "characterCustomization": false,
    "quests": true
  },
  "defaultRecovery": {
    "enabled": false,
    "useExternalDefault": false,
    "appearance": true,
    "quests": false,
    "items": false,
    "progress": false
  }
}
```

Interpretation:

- Mandatory orphan pruning is not controlled by the ledger or baseline toggle.
- Character menu/table extensions remain active when automatic appearance
  persistence is off.
- Quest definitions still load if quest persistence is disabled, but
  RuneSchema quest actions are blocked because native quest progress is
  save-backed.
- Baseline recovery must be explicitly enabled and remains additive.

## 15. Author migration notes

Authors moving from 0.7.6.8 should:

1. Remove `Multiplayer` from NPC definitions. Legacy input is tolerated but no
   longer changes behavior.
2. Give durable items, quests, and recipes stable unique persistence IDs.
3. Prefer full object paths for recipe item inputs/outputs.
4. Keep generated vendor recipes transient; do not treat them as player-learned
   progression.
5. Use direct `DA_` character-option patches, `$AppendUnique`, and
   `$MergeWhere` rather than repeating the old metadata envelope.
6. Let RuneSchema calculate character-option columns; do not hard-code a
   competing grid mutation.
7. Use `$RuntimeWidget` only for existing live widget trees and `$RuntimeUI`
   only for the bounded primitive set.
8. Use `$TextStyle` for whole-widget font/color work. Use a true rich-text
   widget or cooked asset for per-word styling.
9. Install identical gameplay content and cooked assets on server and clients
   that need to render it; do not publish separate single-player/multiplayer
   JSON variants.
10. Put explicit mod order in `runeschema.txt`; do not expect numeric or AA/ZZ
    prefixes to reorder explicit rows.

## 16. Player/operator migration notes

For an upgrade from the Nexus 0.7.6.8 package:

1. Close Dragonwilds and any dedicated server.
2. Back up the complete RuneSchema folder and character-save location.
3. Replace the old RuneSchema directory as one unit; do not overlay only the
   new DLL onto old plugins and PAKs.
4. Preserve authored `mods`, settings, and intentional default baseline data
   separately, then merge them into the new layout deliberately.
5. Verify `RuneSchema : 1` in UE4SS `Mods/mods.txt`.
6. Confirm the startup log reports version 0.7.7.1, the expected storefront,
   native lane, mappings status, and network role.
7. On the first run, review `[PERSISTENCE-PRUNER]` messages before repeatedly
   entering/exiting worlds.
8. Keep generated `.runeschema-startup-*.bak` files until the character has
   loaded, saved, exited, and re-entered successfully.
9. Do not directly edit Game Pass WGS payloads. Use a provider-aware converter
   for manual recovery.

## 17. Known limitations and open risks

- This is an experimental build, not the current Nexus Community Edition
  package.
- Game updates can invalidate storefront-specific native signatures or
  reflected layouts. Fail-closed behavior reduces risk but cannot guarantee
  forward compatibility.
- Cleanup cannot prove an ID is orphaned until the applicable live registry is
  complete and stable. In that state RuneSchema intentionally leaves the save
  unchanged.
- The once-per-process preflight means a Game Pass session should be restarted
  before testing cleanup on a different character. Steam's startup pass checks
  all bounded local character JSON files.
- Baseline recovery can only restore structures implemented by its explicit
  appearance/items/quests/progress merge rules. It is not a general save merger.
- Per-word color/style is unavailable on plain text widgets.
- `$RuntimeUI` is deliberately limited and cannot replace cooked complex menus.
- Dedicated/server and client content still must be distributed consistently
  for replicated presentation and cooked asset availability.

## 18. File-level implementation map

| Area | Primary implementation |
|---|---|
| Orphan decision and baseline merge | `raw/include/Core/SaveCleanup.h` |
| Once-per-process pruning and external baseline | `raw/src/Core/PersistencePruner.cpp` |
| Startup file pass, live registry capture, load preflight | `raw/src/Misc/DragonWildsDataRegistrar.cpp` |
| Baked appearance baseline | `raw/src/Core/AppearanceDefaults.cpp` |
| Diagnostic ledger | `raw/include/Core/PersistenceDiagnosticLedger.h` |
| Dialogue native state | `raw/include/Loader/NativeDialogueSave.h` |
| Quest live-registry resolution | `raw/include/Loader/QuestNativeRegistry.h` |
| Item clone and customization layout application | `raw/src/Loader/DragonWildsAssetModLoader.cpp` |
| Character grid limits | `raw/include/Loader/CharacterCustomizationLayout.h` |
| Recipe persistence and vendor isolation | `raw/src/Loader/DragonWildsRecipeModLoader.cpp` |
| Runtime widgets/UI/text style | `raw/src/Loader/DragonWildsBlueprintModLoader.cpp` |
| NPC normalization/network policy | `raw/include/Loader/NpcCatalog.h`, `NpcNetwork.h` |
| Mod discovery | `raw/include/Utility/ModFolderLayout.h` |
| Authoritative load order | `raw/src/Loader/ModLoadOrder.cpp` |
| Plugin compatibility and PAK policy | `raw/include/Runtime/PluginCatalog.h` |
| Storefront detection/signatures | `raw/include/Runtime/Storefront.h`, `raw/include/SDK/DragonWildsSignatures.h` |
| Fatal/partial startup handling | `raw/src/Loader/DragonWildsMainLoader.cpp`, `raw/src/Utility/StartupTrace.cpp`, `raw/src/dllmain.cpp` |
| Build and packaging | `../build/build.ps1`, `../build/mappings.lock.json` |

## 19. Final assessment

Relative to Nexus Community Edition 0.7.6.8, experimental 0.7.7.1 changes
RuneSchema from a framework that could create valid content but still had
timing-sensitive save and world-reentry failure modes into a registry-first,
storefront-aware runtime with explicit lifecycle boundaries.

The most consequential fixes are stricter proof requirements: content registers
before cleanup; a live identity must round-trip; incomplete registries cannot
authorize a mutation; quest/dialogue cleanup waits for quest registration;
cleanup runs once; and every actual removal is logged. The new UI and authoring
features are layered on top of those rules rather than being permitted to
weaken them.
