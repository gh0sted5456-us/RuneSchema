# RuneSchema 0.6.1E

**UE4SS Stack Limit Fix Dependent**

RuneSchema 0.6.1E is released by **Jonesing4Space**. It extends RuneSchema 0.6.0 by **Snorkles**, which is based on **PalSchema** by Okaetsu.

## What 0.6.1E provides

- JSON and JSONC mod loaders for assets, raw tables, Blueprints, recipes, journal entries, buildings, spawns, courses, strings, and player rules.
- `$Patch` field updates and clone support with deterministic `AA_` and `ZZ_` mod ordering.
- Asset-authored appearance effects for equipment, characters, spawns, Blueprints, weapons, armor, and resource nodes.
- Equipment runtime behavior for validated Surge evade and Shadowveil stealth rules, with separate client and dedicated-server contracts.
- Native AI spawns, resource nodes, grounding, loot rows, display names, combat scaling, drop scaling, and visual effects.
- Manual inspection, export, trace, and preset tools. Diagnostics are opt-in and may affect performance.
- Consolidated normal logging. Spawn output reports counts for AI, bosses, resource nodes, other actors, and removals. Detailed success narration requires **Advanced verbose logging**.
- Preferred cooked-content layout: `mods/<ModName>/paks/<PackName>/`. The older named-folder layout remains supported.

## Installation

Install the matching UE4SS StackFix host supplied with this release, then place RuneSchema under `ue4ss/Mods/RuneSchema/`. Replace DLLs only while the game and dedicated server are stopped.

`version.dll` in the supplied UE4SS package is the dedicated-server entry point. Single-player clients ignore it.

## Authoring boundaries

Use `/assets` for item properties, cloned item data, perk icons, recipes, and appearance effects. Use `/equipment` only for validated runtime behavior that data edits cannot express. Use `/spawns` for world actors and AI spawn points.

## Status

0.6.1E is an experimental release candidate. A matching game executable and matching UE4SS host are required for native equipment behavior. Repeated client and server launches, world entry, GUI use, and normal exit remain the practical validation for a mod set.

---

## Contents

1. [What 0.6.1E changes](#1-what-061e-changes)
2. [Runtime layout and mod folders](#2-runtime-layout-and-mod-folders)
3. [Load order and lifecycle](#3-load-order-and-lifecycle)
4. [Authoring assets and equipment](#4-authoring-assets-and-equipment)
5. [Field patches and reflected arrays](#5-field-patches-and-reflected-arrays)
6. [Appearance and ghost rendering](#6-appearance-and-ghost-rendering)
7. [Manual tools, presets, and tracing](#7-manual-tools-presets-and-tracing)
8. [Runtime efficiency and readiness changes](#8-runtime-efficiency-and-readiness-changes)
9. [Implementation and ownership constraints](#9-implementation-and-ownership-constraints)
10. [Build system and dependency boundary](#10-build-system-and-dependency-boundary)
11. [Validation, known limits, and test checklist](#11-validation-known-limits-and-test-checklist)
12. [Consolidated 0.6.1E changelog](#12-consolidated-061e-changelog)

---

# 1. What 0.6.1E changes

RuneSchema 0.6.1E focuses on safer initialization, clearer authoring, more efficient synchronous loading, improved diagnostics, and cleaner dependency boundaries without changing the established loader order.

The major themes are:

- **Readiness gating.** Core loader/reflection work now waits for UE4SS readiness and a game-thread lifecycle event. Early DataTables are queued by address and replayed only after safe live-object validation.
- **Early-table replay correction.** Live DataTables with object serial `0` are accepted during replay instead of being silently lost through an unsuitable weak-pointer conversion.
- **More efficient asset draining.** Completed asset documents are compacted in one stable pass instead of repeatedly erasing from the front of a vector.
- **Reduced duplicate work.** Synchronous asset batches perform one item-subsystem lookup, and successful initial journal passes are not redundantly repeated for every journal-owning mod.
- **Cleaner logging.** Detailed per-item startup narration and startup trace file I/O are behind the existing off-by-default advanced verbose setting. Normal output keeps actionable totals, warnings, and errors.
- **Improved mod layout.** `paks/<pack-name>/` is the preferred layout when one RuneSchema mod contains multiple cooked packs, while flat and legacy layouts remain compatible.
- **Expanded authoring.** Reflected struct arrays can be edited with selector-based `$Patch` operations, and `$VisualEffect` supports authored ghost rendering for players, spawns, blueprints, and equipment.
- **Equipment runtime behavior.** `/equipment` is reserved for validated runtime behaviors that cannot be expressed as ordinary asset data; native effect grants and item properties remain in `/assets`. Surge and Shadowveil include version-checked profiles for the supported Windows client and dedicated-server executables.
- **Diagnostic depth controls.** Preset capture defaults and limits were expanded while keeping deeper traversal an explicit per-session opt-in.
- **Dependency boundary cleanup.** Engine-independent JSON file/value parsing and patch validation moved into a statically linked `RuneSchemaJsonCore`, while Unreal reflection and conversion remain in the runtime adapter.
- **Build accountability.** Release builds require a full rebuild, dependency/input fingerprinting, retained logs/maps, and explicit verification of the UE4SS imports used by the produced DLL.

0.6.1E does **not** claim to remove UE4SS, replace its lifecycle, eliminate every startup pause, solve every heap-corruption report, or provide cross-version compatibility for native hooks after the game executable changes. The supported Surge and Shadowveil client/server profiles are version-checked and fail closed when their executable contract does not match. The existing Dragonwilds compatibility bootstrap, pak interception, loader order, and gameplay JSON semantics are intentionally preserved unless a section below says otherwise.

---

# 2. Runtime layout and mod folders

## 2.1 Preferred mod structure

RuneSchema continues to use the existing `RuneSchema/mods/` root. JSON authoring folders retain their established roles. For cooked content, the preferred organization is now a named directory under `paks/`:

```text
RuneSchema/mods/GravetideStaff/
  assets/
  recipes/
  paks/
    GravetideStaff/
      GravetideStaff.pak
      GravetideStaff.utoc
      GravetideStaff.ucas
    AnotherPack/
      AnotherPack.pak
      AnotherPack.utoc
      AnotherPack.ucas
```

Use only the container files actually produced by the pack build. A pak-only build does not need fabricated `.utoc` or `.ucas` companions. Keep signatures and genuine companion files together with their original names.

The following layouts remain compatible:

- `mods/<ModName>/paks/<containers>` — existing flat layout.
- `mods/<ModName>/paks/<PackName>/<containers>` — preferred multi-pack layout.
- `mods/<ModName>/<LegacyNamedFolder>/<containers>` — legacy compatibility layout.

Do not install duplicate copies of the same cooked containers in more than one compatible layout.

## 2.2 Cooked paths are unchanged by folder organization

External folders only organize files on disk. They do not rewrite cooked object paths such as `/Game/Mods/...` or plugin mount paths contained inside a pak. JSON references must continue to use the actual cooked paths.

FModel exports and uncooked content directories are not substitutes for packaged `.pak`, `.utoc`, or `.ucas` files.

## 2.3 Pak discovery

RuneSchema's existing native `GetPakFolders` hook registers the `RuneSchema/mods/` root with Unreal. Unreal performs recursive pak discovery beneath that root. RuneSchema does not add a second pak scanner for the preferred layout.

Unknown-folder validation recognizes canonical JSON folders and `paks`. Legacy named folders are accepted when they contain a real pak/IoStore/signature file. The compatibility scan uses filesystem error-code iteration, checks for regular files, and stops after finding qualifying content.

Folder names that merely end in a container-like extension do not count as mounted content.

## 2.4 Pak mount order is not JSON mod order

The `AA_` / normal / `ZZ_` ordering rules described later affect RuneSchema JSON loading. They are **not** a pak mount-priority system. Native Unreal pak naming and mount rules still determine priority when cooked containers overlap.

RuneSchema's current root-level pak registration also does not filter mounted containers based on a mod's JSON enable/disable setting. If cooked content must not mount, keep the disabled or backup containers outside the registered `mods` tree.

---

# 3. Load order and lifecycle

## 3.1 JSON mod order

RuneSchema preserves the established case-insensitive mod ordering:

1. `AA_` folders first.
2. Normal mod folders next.
3. `ZZ_` folders last.

Order remains stable within each group. Use `ZZ_` for patch mods that must apply after the base definitions they modify.

Changing mod order, adding appearance rules, changing blueprint rules, or deleting files generally requires a restart for a complete replay. Auto-reload can update supported live data, but it is not a complete replacement for startup ordering and streaming behavior.

## 3.2 Readiness gate

The early DataTable detour records **addresses only** until UE4SS has called `on_unreal_init`. It does not perform table reflection, property edits, or class-cache setup before that readiness signal.

The early queue:

- preserves arrival order;
- deduplicates addresses;
- accepts reentrant table events while core setup is in progress; and
- is capped at 4,096 entries.

If the queue overflows, RuneSchema treats initialization as failed for that run instead of silently losing table edits.

After UE4SS readiness, RuneSchema installs the existing GameInstance hook. Core initialization begins from the next game-thread DataTable event or from the GameInstance callback if no suitable table event occurs. Core loader work is not intentionally moved onto the UE4SS initialization thread.

## 3.3 Early-table validation and replay

Once core setup completes, RuneSchema performs one live-object validation walk. Early addresses are **not dereferenced** during this discovery pass. Matching DataTables receive a temporary object-array index and serial snapshot.

Replay then revalidates, immediately before registration:

- object-array slot;
- object address;
- serial value; and
- live object flags.

Serial `0` is valid in this replay path. The temporary snapshot is not used as a persistent weak pointer.

Queued tables are replayed in their original arrival order. Removed or replaced objects are skipped and reported. The queue and temporary lookup storage are released after replay. There is no new periodic scan or tick hook.

The live-object discovery walk stops early once every queued address has a unique valid snapshot. Missing, invalid, or non-table addresses still require a full walk. Duplicate object visits do not satisfy more than one queued identity.

## 3.4 Loader ownership and shutdown

The main loader stops and joins its file watcher before unregistering callbacks or destroying loaders. File watching queues paths; engine work runs on the game thread.

Loaders are destroyed while their table registry still exists. Registry callback snapshots execute outside the collection lock so callbacks can add or remove registrations without holding that collection lock through user work.

Inline detours publish their trampolines before enabling hooks. Blueprint callbacks do not write until both required hooks are ready. Surge and Shadowveil verify all required executable sites before activating selected patches. Exceptions must not unwind through native game callers.

These documentation changes do not alter phase ordering, raw-table callback order, or AA_/normal/ZZ_ sequencing.

---

# 4. Authoring assets and equipment

## 4.1 Keep data in `/assets` when data is sufficient

Native effect grants, equipment properties, and item perk presentation belong in `/assets`. Use the existing asset fields such as `GrantedEffects` and `BuffDatas`.

Native equipment remains responsible for applying and removing the effects it owns. The `/equipment` loader does **not** create assets, grant effects, or duplicate ordinary data patches.

Use `/equipment` only for validated runtime behavior that cannot be expressed as a normal asset edit.

## 4.2 Shadowveil action protection

0.6.1E supports per-wearable Shadowveil preservation rules:

```json
{
  "ShadowveilWearables": {
    "/Game/RuneSchema/ShadowPaladinSet/Items/rs_shadow_paladin_cloak.rs_shadow_paladin_cloak": {
      "PreserveOn": [
        "MeleeAttack",
        "RangedAttack",
        "MagicAttack",
        "UtilityCast",
        "Evade"
      ]
    }
  }
}
```

Choose any supported subset. The list for an item replaces that item's earlier list. `false` or an empty list disables preservation for that item.

Other native removal causes, including damage and interactions, remain active. Unequipping must still remove the effect.

Existing `SurgeEvadeLegs` maps are unchanged. Legacy `ShadowveilAttackEvadeWearables` boolean maps preserve their existing melee/ranged/evade behavior. Use only one Shadowveil format per document.

Each behavior accepts up to 64 exact asset paths. Unknown fields, unknown actions, duplicates, invalid paths, or capacity violations reject the entire document rather than partially accepting it.

Later files in existing mod order override earlier entries.

The handler validates the native effect class, GE data class, wearable source, and reflected source item data. It skips only selected delegate registrations. It does not suppress general removal, poll equipment, invoke arbitrary `ProcessEvent`, retain UObject pointers, or automatically reapply effects.

Melee, ranged, evade, combat-magic, utility-cast, and source matching are supported by the 0.6.1E rule format. Combat-magic and utility-cast behavior should be verified against the target game build because those hooks are version-sensitive.

## 4.3 Cloak bonus and item perk display

A typical 0.6.1E patch can append a native equipment effect to a cloak in `/assets`, preserving existing grants with `$Append`.

The included Shadowveil action-protection example appends the native Ulv's Longbow `DamageModifierPlayerAttackingUnalertedAI` behavior to the mantle. Exported data describes an infinite `1.15` multiplier, or 15% additional damage against unalerted enemies.

Remaining invisible does not necessarily return an already alerted enemy to an unalerted state.

The example also adds a **Veiled Ambush** item-perk description using the game's Attack skill icon through `BuffDatas.BuffIcon`. This is item-perk presentation, not a new HUD status icon and not a rewrite of the shared gameplay-effect presentation defaults.

The Blueprint patch that makes Shadowveil itself infinite is separate and global. Wearable protection does not apply to ordinary cast Shadowveil unless the effect source resolves to a configured wearable.

## 4.4 Clone identity and runtime paths

A cloned wearable can retain the source stats table while using a stats row named after its own `InternalName`. A `/raw` patch can therefore provide distinct stats without duplicating the full DataTable row handle in `/assets`.

For cloned items, the authoring identity used by `$Patch` and the resulting runtime asset path used by `/equipment` may intentionally differ.

Equipment hook installation does not require the target wearable asset to already be loaded. The runtime handler matches the source item when the game later grants the effect.

## 4.5 Client and dedicated-server equipment profiles

0.6.1E includes native Surge and Shadowveil hook profiles for the supported Windows client and dedicated-server executables. This extends the same equipment callback logic to the dedicated server without changing mod JSON, boot order, loot patch behavior, or the authoring format described above.

| Executable | PE timestamp | SizeOfImage | SHA256 |
|---|---:|---:|---|
| `RSDragonwilds-Win64-Shipping.exe` | 3306174033 | 230744064 | `1d9d140943604c22b9b50065a00e87b20e448fced8f53e222addbe2177924f86` |
| `RSDragonwildsServer-Win64-Shipping.exe` | 1580929729 | 207376384 | `1c83a69084cecbd3ce8412b8efbc43c505c9b88eac303ddda0289b22285b29bc` |

The SHA256 values identify the analyzed executables for documentation and reproducibility. Startup profile selection uses the existing PE timestamp and `SizeOfImage` contract, followed by exact 32-byte checks at every required hook site and every nonzero resume site. RuneSchema does not hash the entire executable during startup.

All required sites are validated before hook creation. An unknown executable, changed bytes, an invalid resume window, or hook creation/activation failure leaves the affected feature disabled rather than falling back to another executable's offsets. There is no runtime pattern scan and no client-offset fallback on a dedicated server.

The shared `NativeHookContract` performs profile selection, overflow-safe bounds checks, and hook/resume byte validation. Surge and Shadowveil keep their existing callbacks; the executable profiles only define and validate where those callbacks may attach. Disabled diagnostics identify the failed stage, such as an unsupported build, changed bytes, or hook creation/activation failure.

The supported server profile was derived by comparing the relevant Surge and Shadowveil function sites and resume windows against the client implementation. That analysis supports reuse of the existing callbacks, but live multiplayer behavior remains the final validation step because matching normalized instruction structure does not prove that every external callee or global behaves identically.

When validating a dedicated server, confirm the log reports `Equipment Surge (server): enabled` and `Equipment Shadowveil (server): enabled`, then exercise equip/unequip, melee, ranged, magic, utility, evade, reconnect, equipment swaps, and normal shutdown with a compatible client. Native effects may require compatible client behavior in addition to server authority.

---

# 5. Field patches and reflected arrays

0.6.1E extends the reflected property writer with selector-based edits for existing **arrays of structs**. This is available through `/blueprints`, `/raw`, and the generic `/assets` property writer.

It is not a new generic merge operator for every RuneSchema definition type, and it does not change existing custom recipe, journal, player, or spawn parser semantics.

## 5.1 Blueprint patch envelope

Blueprints accept the same outer patch identity envelope used by other loaders:

```json
{
  "$Patch": "BP_FellableTree_Ash_C",
  "$Target": {
    "ItemDropOnSplitComponent": {
      "ItemsToDrop": {
        "$Patch": [
          {
            "$Match": {
              "ItemDataClass": "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash"
            },
            "$Target": {
              "MinToDrop": 3,
              "MaxToDrop": 5
            }
          }
        ]
      }
    }
  }
}
```

The outer `$Patch` identifies a Blueprint class using one of the supported forms:

- short class name ending in `_C`;
- full object path such as `/Game/.../Asset.Asset_C`; or
- package path such as `/Game/.../Asset`.

`$Target` contains only fields to assign.

Blueprint patches apply after ordinary Blueprint rules, in patch load order. Exact identity matching is used, including when short-name and full-path rules overlap.

A Blueprint document may contain one patch envelope, an array of envelopes, or named envelopes inside the established Blueprint document map.

## 5.2 Struct-array selectors

Inside an existing reflected struct array, use either:

- `$Match` with a non-empty object of field/value selectors; or
- `$Index` with a zero-based array index.

Every selected edit must contain a non-empty `$Target` object.

All fields supplied to `$Match` must compare equal. Reference fields should use canonical Unreal object-path strings, not FModel numeric export suffixes such as `.0`.

Prefer selectors based on stable identities such as item references rather than on a quantity the same patch intends to change.

Exactly one entry must match. Missing, ambiguous, or out-of-range selectors fail without appending, clearing, or silently choosing an arbitrary entry.

Unknown fields inside an array edit are rejected.

## 5.3 Atomicity and nesting

All edits for one reflected array are staged in a deep reflected copy and committed together. This gives **array-level atomicity** only; it is not a transaction across an entire Blueprint, DataTable, or mod.

Unmentioned fields and unselected entries survive.

Nested struct edits and nested struct-array patches are supported. `$Match` does not traverse UObject properties and does not match array fields. The initial operation is intended for arrays of structs, not arbitrary scalar or UObject arrays.

Existing literal arrays continue to replace arrays. Existing `Items`, `Action`, and `$Append` behavior is unchanged. Do not mix clear/append commands with an array `$Patch` envelope for the same operation.

## 5.4 Raw DataTable patches

Raw patches retain the `DataTable:RowName` identity and can use the same selector syntax:

```json
{
  "$Patch": "DT_LootDropTable:Wolf",
  "$Target": {
    "Resources": {
      "$Patch": [
        {
          "$Match": {
            "SpawnedItemData": "/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Monstrous_Fang.ITEM_Resources_Monstrous_Fang"
          },
          "$Target": {
            "MinimumDropAmount": 2,
            "MaximumDropAmount": 3
          }
        }
      ]
    }
  }
}
```

A missing raw row is not created by a patch. A selector patch preserves unrelated array entries, unrelated rows, and unmentioned fields such as drop chances and flags.

If another mod or native data produces a second matching entry, refine `$Match` with additional stable fields or inspect the final array and use `$Index`.

Raw auto-reload can update a currently loaded table. Restart for a complete load-order and streaming replay.

Blueprint patch edits require a restart; auto-reload reports that limitation.

---

# 6. Appearance and ghost rendering

Ghost rendering and overlay glow use RuneSchema's existing material-rendering path. They are cosmetic material changes, not spell enchantments, character-creation skin colors, arbitrary particle effects, or nameplate/widget effects.

## 6.1 Ghost style fields

A ghost style can contain:

```json
{
  "Type": "Ghost",
  "Overlay": true,
  "BodyMaterial": true,
  "MainColor": {
    "R": 0.2,
    "G": 0.8,
    "B": 1.0,
    "A": 1.0
  },
  "SecondaryColor": {
    "R": 0.05,
    "G": 0.2,
    "B": 0.5,
    "A": 1.0
  }
}
```

`Overlay` defaults to `true` and enables the ghost shader's glow overlay.

`BodyMaterial` defaults to `false`. When `true`, RuneSchema also replaces physical mesh material slots with the baked ghost body material.

Either layer can be used alone or both can be enabled together. A rule with both `Overlay` and `BodyMaterial` disabled is rejected; remove or null the rule instead.

RGBA colors retain the established ranges.

Widget components and nameplates are excluded from physical mesh processing.

## 6.2 Players and spawns

For `/players` and `/spawns`, author the effect through `VisualEffect`.

A player rule can target:

- `PlayerMesh` — body/head only; or
- `EntirePerson` — the default, including armor and held items.

A PlayerMesh effect can coexist with independently styled equipment.

Character appearance ownership is layered. A specific equipment rule takes precedence for that item's mesh. Removing the specific equipment rule allows a broader player rule to become visible again if one exists.

Shadowveil stealth takes priority while active. Authored appearance resumes through the existing appearance-refresh events.

## 6.3 Equipment appearance

For equipment, place `$VisualEffect` inside the item's `/assets` definition or patch:

```json
{
  "$Patch": "/Game/Gameplay/Character/Player/Equipment/Cape/ITEM_Cape_Artisan.ITEM_Cape_Artisan",
  "$Target": {
    "$VisualEffect": {
      "Type": "Ghost",
      "Overlay": true,
      "BodyMaterial": true
    }
  }
}
```

The same metadata field can be authored beside `$Clone`.

`$VisualEffect` is RuneSchema metadata. It is not written into Unreal as an unknown reflected property.

Equipment rules automatically apply to the item's worn head/body/legs/cape mesh or to physical meshes on its held left/right actor. Do not provide a separate appearance `Target` for an equipment item.

Inventory icons and hidden/stowed models are not changed.

Put late equipment appearance patches in `ZZ_` mod folders when they must override earlier definitions.

`$VisualEffect: null` removes that equipment-specific rule. It does not delete the item and does not erase a broader player rule.

Keep the World/player runtime loader enabled so equipment refresh events can re-evaluate appearance. The `/equipment` runtime-behavior loader is not required for cosmetic effects.

## 6.4 Blueprint appearance

Blueprints can apply the same metadata through a patch envelope:

```json
{
  "$Patch": "BP_Tree_Ash_01_C",
  "$Target": {
    "$VisualEffect": {
      "Type": "Ghost",
      "BodyMaterial": false,
      "MainColor": {
        "R": 0.2,
        "G": 0.8,
        "B": 0.95,
        "A": 1.0
      }
    }
  }
}
```

The metadata is removed from the ordinary reflected-property assignment before Blueprint fields are written.

The effect uses the existing actor-initialization observer after component changes. It is not written into class defaults. Matching current or future streamed actor instances receive the effect when initialized; RuneSchema does not add polling for this feature.

`BodyMaterial: true` replaces existing body slots with the baked ghost body material, using the configured color pair. Overlay colors continue to use the established top-color parameters. Existing fade/custom primitive data behavior is not rewritten by the visual-effect authoring layer.

Setting Blueprint `$VisualEffect` to `null` clears the configured rule for newly initialized actors. Restart to reliably restore already modified actor materials.

Streaming foliage that is not represented as an actor requires a different pathway and is not covered by this feature.

## 6.5 Material ownership, caching, and call efficiency

RuneSchema records original materials and restores only slots that still contain RuneSchema-owned replacements. This avoids erasing a material another system wrote after RuneSchema.

Mesh changes capture the new material state. Outgoing equipment restores only the slots still owned by RuneSchema.

Identical visual styles share material instances. Streamed Blueprint actors use GameInstance-owned shared materials so one actor can unload while others still use the same style.

Appearance rule keys are prepared when a rule is applied or an item effect is set, rather than repeatedly copied and serialized during palette lookup. `Target` remains excluded from the key, while the original rule values remain available for scope and material creation.

For each mesh operation, `GetMaterial` and `SetMaterial` resolve their UFunction and allocate scalar/pointer-only argument storage at most once, then reuse those calls across the mesh's material slots. The function pointers and argument buffers live only on the stack for that operation; RuneSchema does not cache them across worlds or retain owning Unreal parameter values. Slots already using the correct material are not rewritten, and reflection invocation/parameter checks remain active.

Class-property lookup now filters candidate properties by `FName` before converting candidate names to text. Exact string comparison, inherited/shadowed-property handling, and last-match behavior are preserved. This reduces avoidable name-conversion work without skipping the linked-list property walk. Script-struct lookup was already `FName`-based and is unchanged.

Appearance hooks use the shared `NativeHookContract` validator, including appearance sites that do not have resume windows. Existing client rendering byte contracts and callbacks remain unchanged. Superseded standalone material-call wrappers, per-refresh style normalization, and duplicate appearance bounds/byte-comparison work were removed.

Player material ownership is bounded. Spawn and Blueprint style caches are limited to 256 styles. Actor tracking uses weak identities to avoid address-reuse mistakes. Weak visual/material ownership and teardown barriers are unchanged by these optimizations.

Dedicated servers skip cosmetic material loading and appearance hooks. Rendering clients need the appearance JSON. Cloned item identities and recipes still need to be installed consistently where gameplay requires them. Native Surge and Shadowveil equipment behavior is separate from cosmetic appearance and has its own dedicated-server executable profile described in section 4.5.

## 6.6 Included examples

Optional examples in the 0.6.1E workspace include:

- **GhostlyWarden** — four separate craftable equipment clones retaining their base stats while applying ghost appearance.
- **GhostStaff** — an overlay applied to the observed Pharaoh's Sceptre.
- **GhostAshTrees** — ghost rendering for verified standing/fellable ash actor classes.

Examples are not automatically enabled.

---

# 7. Manual tools, presets, and tracing

RuneSchema's diagnostics are deliberately manual. Tools are disabled each launch and must be activated from the RuneSchema tab in UE4SS.

Heavy reflection or broad captures can stall the game or increase memory use. Restarting the game after a large diagnostic session may be appropriate.

## 7.1 Tools interface

The Tools area contains:

- **Inspector**
- **Presets**
- **Traces**
- **Results**
- **Exports**

Settings and Tools use independently scrolling regions. General, Loaders, Logging, Spawns, and Load Order also scroll independently. Loader toggles and notification toggles have separate control IDs.

### Inspector

The Inspector can capture the camera's first blocking target or the local player and search loaded object names. The selected result is highlighted and its object path is displayed.

Search is capped at 250 matches. Captures are snapshots, not continuous polling.

### Presets

Presets provide a saved-preset selector, reload control, run control, and JSON editor.

Saving a preset under an existing `Name` replaces that saved preset.

Running a preset produces a timestamped diagnostics result.

### Results

Results shows the latest snapshot with filtering, copy, and JSON export controls. Large JSON is formatted once per changed snapshot rather than repeatedly each frame.

### Exports

Exports includes schema generation and the native equipment/spell API export. Enter a world before running exports that depend on live DataTables or player state.

Appearance is authored through JSON. There is no ghost-preview toggle or session appearance override. Enabling Tools does not enable gameplay appearance behavior.

## 7.2 Reusable property-capture presets

A preset can define bounded reflected reads:

```json
{
  "Name": "MyObjectScope",
  "PropertyCaptures": [
    {
      "Root": "Player",
      "Path": ["PlayerCombatMagicComponent", "*"]
    }
  ],
  "CaptureLimits": {
    "MaxDepth": 5,
    "MaxEntries": 128,
    "MaxNodes": 8192,
    "MaxSparseSlots": 4096,
    "FollowObjectReferences": true
  }
}
```

Supported roots are:

- `Player`
- `Controller`
- `Selected`
- a full Unreal object path

`Selected` uses the last inspected actor/object or object-search selection. Its path is stored when **Run** is clicked and resolved again on the game thread.

A path contains 1-8 reflected property names. The final segment may be a specific field or `*` for all fields on the final object. Intermediate wildcards, function calls, and writes are rejected.

`PropertyCaptures` allows up to 32 targets and can coexist with legacy preset fields.

The reader supports bounded inspection of:

- scalar and enum values;
- names and strings;
- resolved object references;
- structs;
- arrays; and
- maps.

Unsupported or engine-specific wrappers may be reported as `metadataOnly` rather than force-read. Soft or weak references are followed only when already resolved. Cycles and previously visited objects are linked by path rather than recursively expanded forever.

## 7.3 Capture limits

0.6.1E uses the following policy:

- Unspecified `MaxDepth`: **7**
- Normal accepted `MaxDepth`: **1-10**
- Session deep-capture opt-in: **up to 16**
- Default `MaxEntries`: **64**
- Maximum `MaxEntries`: **512**
- Default `MaxNodes`: **2,048**
- Maximum `MaxNodes`: **16,384**
- Default `MaxSparseSlots`: **4,096**
- Maximum `MaxSparseSlots`: **16,384**
- String output cap: **4,096 characters**
- Saved-preset capacity: **64 presets**

The session deep-capture checkbox resets when the DLL starts again. Changing it reloads preset validation. Load, save, and execution all use the same current depth policy.

Following object references remains opt-in. Traversal does not intentionally load new assets. Deeper limits bound traversal size but do not guarantee a wall-clock maximum or crash safety for every native layout.

Skipped presets report their filenames and validation errors, up to 64 displayed errors. Preset-capacity exhaustion is reported separately.

## 7.4 Included research presets

The 0.6.1E research set includes:

- `Anima05RuneMaps` — exact primary/secondary equipped-ammo map entries.
- `Anima06RuneData` — bounded rune data and a known utility-cost module.
- `Anima07SpellGraph` — optional broader reference following from rune maps into rune/spell data.
- `AppearanceState` — cloak/held-item identities, visible meshes, material lists, native hide-material state, and live effect references.
- `AppearanceMaterials` — resolved body/outfit/cape material properties for before/after comparison.
- `RocksplosionDamage` — normal/upgraded Blueprint damage-component templates.
- `SelectedObjectScope` — a focused capture for an inspected placed object such as a building piece.

These are research captures, not completed gameplay fixes. Use omission/type reports to decide the next targeted investigation instead of repeatedly increasing capture breadth.

## 7.5 Manual traces

### Player Trace

Player Trace records reflected `ProcessEvent` completion for the starting player/controller and their Outer-owned objects on the starting game thread.

Category labels are inferred from function names and are filters, not authoritative engine categories. Direct native calls, external world actors, and separately owned held actors may not appear.

Parameters and call nesting are not captured.

Limits:

- maximum duration: 60 seconds;
- maximum events: 4,096;
- tick suppression: enabled by default.

Manual action markers can label test steps.

### Appearance trace

Appearance trace observes five version-checked native appearance sites and records up to 256 events over 60 seconds, with before/after captures around manual start/stop.

Stopping detaches the recorder. Shared appearance hooks remain installed only when authored player/equipment appearance still requires them.

The trace observes material behavior; it does not change materials itself.

Both trace systems are bounded manual diagnostics. Only live gameplay can establish their coverage for a specific game build.

---

# 8. Runtime efficiency and readiness changes

## 8.1 Asset queue compaction

Asset queue draining now uses stable linear compaction instead of repeatedly erasing completed entries from the front of the queue. Repeated front erasure can require `N*(N-1)/2` entry moves for `N` completed entries; 0.6.1E avoids that pattern.

- processing order is unchanged;
- unresolved entries are retained;
- completed entries are discarded;
- retained entries move toward the front at most once; and
- the unused suffix is erased after processing.

A fully consumed queue performs no entry moves. Queue capacity is retained and the drain adds no queue allocations.

If processing throws, completed entries remain consumed while unresolved entries, the failing document, and the untouched suffix remain in original order. Native writes that happened before the failure are not rolled back, matching the prior behavior.

Asset resolution, clone identity validation, deferred patches, and error semantics are otherwise unchanged.

Automated queue tests cover retained, consumed, and exception cases to confirm that the optimized drain preserves ordering and retry behavior. The optimization reduces unnecessary data movement; actual startup-time improvement depends on the mod set and game environment.

## 8.2 One item-subsystem lookup per batch

A synchronous `TryApplyPending` asset batch now finds the item subsystem once and passes that pointer through clone creation and registration.

The pointer is local to that batch. It is not retained across frames or worlds.

## 8.3 Journal work

A successful initial journal application no longer reapplies every queued definition once for each journal-owning mod at the same lifecycle phase.

Errors still allow retries. Explicit manual auto-reload remains available. Recipe placement passes were reviewed but retained because later table changes can make another placement pass necessary.

## 8.4 Logging cost

Logging wrappers forward arguments instead of repeatedly copying them. Notification-channel lookup uses string views.

The following are advanced verbose output rather than normal startup narration:

- per-item recipe creation/placement;
- clone identity details;
- detailed Blueprint registration output;
- mod-by-mod loading narration; and
- native signature addresses.

Normal output still reports meaningful clone/patch totals, table summaries, readiness replay counts, warnings, and errors.

`enableDebugLogging` defaults to `false` and is exposed as **Advanced verbose logging**.

Startup trace file rotation, open, write, and flush operations occur only when verbose logging is enabled. Disabled tracing does not rewrite old trace files, so timestamps matter when comparing logs.

Removing ordinary source comments does not reduce DLL size; comments are not compiled. Removing executable logging can remove compiled formatting instructions, but only a rebuilt binary can show the actual effect.

## 8.5 Appearance and reflection work reduction

0.6.1E also reduces repeated work inside appearance and reflected-property operations:

- visual style keys are prepared at apply/set time rather than serialized during every palette lookup;
- one mesh read/write operation resolves and prepares at most one `GetMaterial` call and one `SetMaterial` call, reusing them across all slots;
- already-correct material slots are not rewritten;
- class-property candidates are filtered by `FName` before the comparatively expensive name-to-text conversion; and
- duplicate appearance validation/normalization paths and superseded material-call wrappers were removed.

The optimizations preserve the existing material ownership rules, reflection parameter checks, exact property-match semantics, client-only appearance contract, and event-driven behavior. They do not add polling or cross-world Unreal pointer caches.

Standalone work-count tests verify the reduced operation count. In the test model, a 32-slot material read/write operation constructs two material calls rather than 64, and a 100-property lookup with three case-equivalent candidates converts three names rather than 100. These checks demonstrate reduced work; they are not measured claims about game FPS or startup time.

## 8.6 Startup pauses and crash attribution

RuneSchema and UE4SS both perform initialization and discovery work during startup. Reduced narration and more efficient queue handling can lower avoidable overhead, but they do not guarantee elimination of every startup pause.

Windows heap-corruption reports identify where corruption was detected, not necessarily which loaded module originally wrote invalid memory. Module presence alone is not attribution. Preserve crash reports and dumps when diagnosing native failures.

0.6.1E therefore treats startup, shutdown, and heap-corruption claims conservatively: source cleanup, reduced logging, and dependency relinking are improvements, not proof of a universal crash fix.

---

# 9. Implementation and ownership constraints

This section captures the non-obvious invariants that should remain visible to maintainers even after source-comment cleanup.

## 9.1 Engine memory and containers

Unreal objects may already be unavailable during DLL static destruction. RuneSchema's engine-cleanup lifetime guard disables engine access before owning static containers destruct, while live-world cleanup can still release roots normally.

For PlayerGhost ownership, the stop barrier must outlive the containers that rely on it. Background listener lifetime must likewise cover the watcher threads it controls.

Unreal map/set allocation must use the engine's padded layout and alignment rules. Adding raw key and value sizes can underallocate a pair.

Sparse container occupancy is not equivalent to live element count. Iteration must respect occupied slot bounds rather than assuming `Num` is the last valid slot.

Reflected array edits use deep copies for staged nested arrays and strings. Reference selectors compare the reference itself; they do not recursively mutate a shared UObject while the array edit is staged.

Soft object paths preserve the full suffix after `:`. String views must be copied using their explicit bounds rather than assuming NUL termination.

## 9.2 Appearance ownership

`$VisualEffect` definitions are metadata keyed by authored identities, not native UObject properties.

Player and equipment appearance share a material owner. Specific item rules override broader whole-person rules for the meshes they own.

Original materials are restored only when the current slot still contains RuneSchema's replacement. This prevents RuneSchema from erasing a later material change owned by another system.

Physical mesh enumeration excludes `WidgetComponent` to prevent nameplate quads from becoming visible ghost rectangles.

Resource actors expose their visible meshes through different component arrangements, so appearance operates on enumerated physical models rather than assuming one fixed component field.

## 9.3 Equipment source matching

Shadowveil wearable matching uses the effect's originating item rather than trusting equipment-slot replication timing. Slot replication can lag an equip notification.

Native Surge behavior reads currently equipped legs through the existing evade component. It does not require a polling loop.

## 9.4 Player data

A character GUID takes precedence over display name when identifying a saved character.

Existing native save values should not be rewritten merely to restate an appearance choice. On the first authored appearance change, an observed saved value should be preserved as the fallback when available. An explicitly authored fallback is used only when no prior value was observed.

Disconnected characters are reconciled when they reconnect.

## 9.5 Health and vitals

Health changes must target an authoritative live component with a positive maximum rather than a template component.

Use the public player attribute pool and reflected health APIs. Calling private health-attribute delegates can terminate the game.

`ModifyMaxHealth` expects an absolute maximum. Maximum health derives from the live `MaxHealthAttribute`, and notifications must follow a change.

`GE_ModifyMaxHealth` uses the game's normal permanent-health path. If a high-level wrapper rejects a call during early pawn initialization, the component fallback must preserve Dominion's native 24-byte effect handle.

Vitals remain outside generic attribute writes because they require dedicated notification behavior.

## 9.6 Spawning and resource state

Blueprint AI spawn points perform game-side construction and director registration that a bare native spawn point may not provide.

Some spawn points can emit AI before the authored GUID is propagated into `SpawnInfo`; RuneSchema therefore performs matching for the emitted instance through existing lifecycle events.

Previously introduced resource nodes preserve their save/respawn depletion interval instead of being refilled by reload.

Agility course orbs use `AgilityCourseComponent`; wall and arrow Blueprints use `AgilityCourse`.

## 9.7 Diagnostics safety

Native trace callbacks store bounded address tokens rather than dereferencing them, allocating JSON, or calling engine APIs from the trace site.

Reflected captures run on the game thread and follow only already resolved references within configured budgets.

Type targets are inspected as schemas rather than treated as live object instances.

Stop a trace recorder before starting reflection/export work that could fail. Keep the latest in-memory result available in the UI even when file export fails.

The equipment/spell API exporter describes native layouts. It is not a save-state or inventory serializer.

---

# 10. Build system and dependency boundary

## 10.1 Full release rebuilds

Every 0.6.1E release build requires a full RuneSchema rebuild. The validated local shipping route is `tools/build-cached.ps1` using the configured MSVC/source cache.

The release build:

1. rebuilds the retained static dependencies;
2. builds and tests `RuneSchemaJsonCore` independently;
3. rebuilds RuneSchema with an explicit library list; and
4. retains logs, hashes, and linker-map information for validation.

Do not package incremental release objects.

The shipping target uses `/MD` and explicit `/O1 /Os` optimization in the validated local pipeline.

## 10.2 Rebuilt libraries

`tools/build-dependencies.ps1` rebuilds the retained local static libraries from source snapshots using isolated object/output directories:

- Zycore
- Zydis
- SafetyHook
- efsw
- fmt
- ImGui

The build records input/output hashes and per-library logs.

UE4SS itself is **not** rebuilt or replaced by this pipeline. RuneSchema links against the installed/validated UE4SS import library, and imports must be checked against the runtime actually used for testing.

C++ ABI compatibility with that host remains a live-runtime requirement beyond successful name resolution.

## 10.3 RuneSchemaJsonCore

0.6.1E separates engine-independent JSON responsibilities into a statically linked core library rather than a second runtime DLL.

`RuneSchemaJsonCore` covers:

- JSON/JSONC file reading;
- primitive value validation;
- patch-envelope validation;
- selector validation; and
- engine-independent merge behavior.

Existing equipment and preset validators are also exercised by the standalone test build.

Game-specific conversion and reflection remain in the runtime adapter, including `FVector`, `FRotator`, `FName`, reflected property validation, and actual application to Unreal objects.

Direct `UE4SSProgram.hpp` use is concentrated in `Runtime/HostServices.cpp` for host working-directory and GUI setup. This is an interface boundary cleanup, not removal of UE4SS.

## 10.4 Standalone core build

A clean standalone core can be configured with a command equivalent to:

```text
cmake -S core -B <fresh-output> -DRUNESCHEMA_JSON_INCLUDE_DIR=<nlohmann-json/include>
```

Then build with a clean-first invocation and run `ctest`.

The standalone core does not require UE4SS headers or libraries.

## 10.5 Explicit shipping link boundary

The validated reduced local pipeline explicitly links:

- UE4SS import library;
- rebuilt Zycore;
- rebuilt Zydis;
- rebuilt SafetyHook;
- rebuilt efsw;
- rebuilt fmt;
- rebuilt ImGui;
- `RuneSchemaJsonCore`; and
- `kernel32`.

Thirty-six unused explicit link entries were removed from the local shipping command, including separate Lua, Unreal helper, parser, assembler, graphics-backend, and redundant Windows library inputs.

Not every removed name necessarily contributed code before removal, so binary-size savings must not be attributed one-for-one to the count of removed library names.

Windows/CRT imports and the UE4SS dependency remain.

The repository-wide top-level CMake route can still receive additional transitive dependencies through upstream UE4SS target propagation. Do not claim that route has the same dependency graph as the reduced local shipping command unless it is separately validated.

## 10.6 Dependency source lock

The local dependency lock fingerprints the recorded dependency source/header trees and fails on drift rather than silently mixing one version's headers with another version's binaries.

The workstation cache is not a portable third-party vendor bundle. The source ZIP therefore does not imply that those absolute cache paths or third-party binaries are included.

A preserved fmt compatibility patch wraps the relevant check macro state. Glaze's top-level pin is aligned with the revision used by the 0.6.1E build workspace.

The dependency build retains a SafetyHook `C4834` warning in `utility.cpp` relating to an ignored memory-protection restoration result. It is recorded rather than suppressed or silently modified. RuneSchema itself is expected to compile without warnings or errors in the validated 0.6.1E shipping build.

## 10.7 Binary size and comments

C++ comments and Markdown files are not compiled into the DLL. Moving long explanations out of source improves maintainability and packaging, but does not directly reduce runtime memory or executable size.

Runtime strings, executable instructions, metadata, linked code, and emitted logging calls can affect the binary. Measure the rebuilt DLL; do not estimate size savings by counting deleted comment characters.


## 10.8 Packaging

The runtime ZIP should contain the RuneSchema runtime DLL in its expected `dlls/main.dll` location plus a concise installation README.

Long-form documentation, implementation notes, and version history belong in the source/documentation package rather than the minimal runtime archive.

PDB symbols should be retained separately for crash analysis.

When validating 0.6.1E, replace only the RuneSchema DLL unless the test specifically concerns the host runtime. Keep the UE4SS runtime constant so failures can be attributed cleanly.

---

# 11. Validation, known limits, and test checklist

## 11.1 Automated validation

0.6.1E includes source-level and standalone validation for:

- JSON/JSONC reading and comment handling;
- deterministic file ordering;
- malformed patch and selector rejection;
- protected identity behavior;
- equipment rule validation;
- client/server native hook profile selection, fingerprint mismatch, hook/resume byte mutation, and invalid-range handling;
- appearance ownership/swap/stealth/coexistence behavior;
- multi-slot material call reuse and prepared-style scope equivalence;
- exact, duplicate, numbered, and missing property lookup behavior;
- native hook overflow, truncation, and mutated-byte contract handling;
- preset bounds;
- asset queue ordering and exception recovery;
- early-table duplicate, missing, replaced, and serial-zero handling;
- readiness concurrency and queue overflow behavior;
- shipping import resolution;
- source-boundary checks; and
- archive and file hash accounting.

These checks establish the expected software contracts. They do not replace live Unreal Engine testing.

## 11.2 Live validation

A normal 0.6.1E validation pass should verify:

- game launch and RuneSchema version reporting;
- early DataTable replay totals;
- expected armor and other raw-table rows;
- infused-ammo behavior where used;
- clone creation and recipe registration;
- raw-table and Blueprint patches;
- world entry and world transition;
- player and equipment appearance behavior;
- Shadowveil action preservation, including MagicAttack and UtilityCast;
- Surge equip/unequip and evade behavior;
- ghost/stealth transitions and appearance coexistence;
- normal game exit;
- dedicated-server Surge and Shadowveil enablement, reconnect, equipment swaps, supported actions, and normal shutdown; and
- multiplayer consistency for cloned gameplay identities and recipes.

A successful compile or standalone test suite does not, by itself, establish that every startup pause, heap-corruption report, shutdown failure, or dedicated-server issue is resolved.

## 11.3 Controlled launch checklist

For a clean validation run:

1. Keep the UE4SS runtime unchanged.
2. Replace only the RuneSchema 0.6.1E DLL.
3. Confirm the 0.6.1E version marker in the log.
4. Keep Advanced verbose logging off for the first ordinary gameplay run.
5. Confirm early-table restored/skipped counts.
6. Confirm expected raw-table rows, clones, and recipes.
7. Enter a world and exercise the changed feature.
8. Transition worlds if relevant, then exit normally.
9. If a failure occurs, repeat with Advanced verbose logging enabled and preserve the resulting startup trace, log, crash report, and dump when available.
10. Do not infer causation from module presence alone.

## 11.4 Known limits

- The appearance layer does not cover arbitrary non-actor foliage instances or arbitrary particle effects. Appearance hooks are client-only.
- Surge and Shadowveil native equipment hooks support the documented executable profiles and fail closed on unknown or changed builds; game updates require profile revalidation.
- Property-capture tools are bounded diagnostics, not unrestricted memory dumping.
- Pak folder organization does not create a new pak mount-priority system.
- The dependency-boundary work does not make RuneSchema standalone from UE4SS.
- Engine-version-sensitive native hooks should be revalidated after a game update.
- The supplied matching UE4SS StackFix host is required for this experimental build.

---

# 12. RuneSchema 0.6.1E changelog

This section records the changes delivered in RuneSchema 0.6.1E. It describes the released behavior directly rather than preserving internal test-build history.

## Runtime initialization and DataTable replay

- Added a readiness gate so core RuneSchema initialization waits for UE4SS readiness and a safe game-thread lifecycle event.
- Early DataTable events are queued by address, deduplicated, bounded, and replayed after core setup.
- Replay validates object-array slot, object address, serial, and live flags immediately before registration.
- Serial `0` is accepted as a valid live-object serial in this one-time replay path.
- Early-table replay preserves original arrival order and reports restored/skipped totals.
- The existing GameInstance hook remains available as a fallback initialization path.

**Why it changed:** initialization work is delayed until the runtime is ready, while DataTables discovered early are preserved instead of being lost.

## Asset loading and runtime efficiency

- Replaced repeated front erasure of completed asset documents with stable linear queue compaction.
- Preserved unresolved documents, retry behavior, failure ordering, and existing patch semantics.
- Reduced item-subsystem discovery to one lookup per synchronous asset batch.
- Removed redundant successful initial journal re-application while preserving retries and manual reload behavior.
- Forwarded logging arguments instead of copying them repeatedly.
- Moved detailed per-item startup narration and signature-address output behind Advanced verbose logging.
- Consolidated normal spawn output into new, altered, error, AI, boss, resource-node, other-actor, and removal counts.
- Consolidated normal equipment output into one Shadowveil and Surge summary; native binding detail requires Advanced verbose logging.
- Startup trace file I/O now occurs only when verbose logging is enabled.

**Why it changed:** 0.6.1E removes avoidable synchronous work and startup noise without changing loader order or gameplay semantics.

## Pak layout and mod organization

- Established `paks/<pack-name>/` as the preferred layout for mods containing multiple cooked packs.
- Preserved flat `paks/` and compatible legacy named-folder layouts.
- Hardened legacy folder inspection with filesystem error handling and regular-file checks.
- Updated unknown-folder validation to recognize the preferred `paks` structure.
- Clarified that JSON load order does not control Unreal pak mount priority.

**Why it changed:** multi-pack mods can be organized cleanly without changing cooked paths or adding a second pak scanner.

## Authoring and reflected patching

- Added selector-based `$Patch` editing for existing reflected arrays of structs.
- Made the feature available through `/blueprints`, `/raw`, and the generic `/assets` property writer.
- Preserved protected identity rules and deep-copy handling for nested reflected values.
- Clarified the split between data-driven `/assets` changes and runtime-only `/equipment` behaviors.

**Why it changed:** authors can modify targeted entries inside reflected struct arrays without replacing an entire array or inventing a separate merge system.

## Appearance and visual effects

- Added `$VisualEffect` authoring for player, spawn, Blueprint, resource, and equipment ghost-style rendering.
- Added independent overlay/body layers where supported.
- Added material sharing and safe restoration rules so RuneSchema only restores material slots it still owns.
- Excluded `WidgetComponent` from physical mesh enumeration to prevent nameplate quads from receiving ghost materials.
- Added dedicated-server cosmetic guards for appearance-only work.
- Added event-driven ghost refresh and bounded manual appearance tracing.
- Prepared appearance style keys at apply/set time instead of serializing them during each palette lookup.
- Reused one prepared `GetMaterial` and one `SetMaterial` reflection call across material slots within each mesh operation.
- Filtered class-property candidates by `FName` before text conversion while preserving exact/last-match semantics.
- Unified appearance hook validation under `NativeHookContract` and removed superseded duplicate material/normalization paths.

**Why it changed:** visual effects can be authored as metadata and applied consistently without uncontrolled polling or unsafe material ownership, while repeated palette, material-call, and property-name work is reduced.

## Equipment runtime behavior

- Added per-wearable Shadowveil preservation rules for supported actions such as melee, ranged, magic, utility casts, and evade.
- Preserved unrelated native removal causes and unequip behavior.
- Matched wearable ownership through the originating effect/item relationship rather than relying solely on equipment-slot replication timing.
- Kept native effect grants and ordinary item properties in `/assets`; `/equipment` remains reserved for validated runtime behaviors.
- Added version-checked Surge and Shadowveil profiles for the supported Windows dedicated-server executable while retaining the supported client profile.
- Added exact hook/resume byte validation and fail-closed behavior for unsupported or changed executables, with no runtime pattern scan or server fallback to client offsets.
- Improved equipment-hook diagnostics so disabled features report the actual failed validation or activation stage.

**Why it changed:** equipment-specific native behavior can be controlled consistently on supported client and dedicated-server builds without duplicating data-driven item definitions, introducing polling loops, or attaching hooks to an unverified executable.

## Diagnostics, presets, and tracing

- Increased focused capture depth and normal preset depth limits.
- Added a higher per-session opt-in diagnostic depth for deliberate deep inspection.
- Unified depth validation across load, save, and run paths.
- Improved skipped-preset and capacity errors.
- Added bounded manual player and appearance trace capture.
- Kept native trace callbacks lightweight by recording bounded address tokens and deferring reflection/export work to safe contexts.

**Why it changed:** deeper investigation is available when explicitly requested, while normal runtime diagnostics remain bounded and predictable.

## Shutdown and memory-safety hardening

- Prevented appearance owners from touching Unreal objects after engine access has been disabled during DLL/static teardown.
- Preserved normal live-world material cleanup.
- Corrected padded Unreal map-pair allocation and sparse-map update/removal handling.
- Hardened inline-hook installation and transactional Blueprint-hook activation.
- Kept exception handling from unwinding through native game callers.

**Why it changed:** native cleanup and container handling now better respect Unreal lifetime, layout, and hook-ownership constraints.

## Build system and dependency boundary

- Added a statically linked `RuneSchemaJsonCore` for engine-independent JSON reading, validation, selector checks, and merge behavior.
- Kept Unreal reflection and game-specific type conversion in the runtime adapter.
- Concentrated direct host-program access in `Runtime/HostServices.cpp`.
- Rebuilt retained static dependencies through the documented dependency pipeline.
- Reduced the explicit local shipping link list to required libraries.
- Added dependency/input fingerprinting, build logs, linker maps, and shipping-library existence checks.
- Required clean full rebuilds for release packaging.

**Why it changed:** the engine-independent logic is easier to test in isolation, and shipping builds are more reproducible and auditable without changing the UE4SS runtime dependency.

## Documentation and packaging

- Consolidated the separate 0.6.1E authoring, runtime, appearance, equipment, pak, patching, tools, diagnostics, build, dependency, and tracing notes into this document.
- Removed internal test-build chronology from release-facing documentation.
- Kept the runtime archive minimal and moved long-form technical documentation to the documentation/source package.
- Retained PDB symbols separately for crash analysis.

**Why it changed:** 0.6.1E now has one release-facing source of truth describing what changed, how the behavior works, and what still requires live verification.

---

## External references

These references describe relevant host/runtime APIs and implementation boundaries; they do not certify RuneSchema behavior on their own.

- PalSchema CMake and loader implementation: <https://github.com/Okaetsu/PalSchema>
- PalSchema installation requirements: <https://okaetsu.github.io/PalSchema/docs/gettingstarted>
- UE4SS C++ mod lifecycle guide: <https://docs.ue4ss.com/guides/creating-a-c%2B%2B-mod.html>
- UE4SS BPModLoaderMod / BPML GenericFunctions: <https://github.com/UE4SS-RE/RE-UE4SS>
- Unreal Engine `FPakPlatformFile` API: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/PakFile/FPakPlatformFile>

---

**End of RuneSchema 0.6.1E consolidated documentation.**
