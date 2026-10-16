# RuneSchema 7.8 alpha consolidated changelog

October 13–16, 2026. This consolidates the 7.8 alpha work, including the source
published as [`3cbbaee` — update](https://github.com/gh0sted5456-us/RuneSchema/commit/3cbbaeeb58b6af932caa952854f4ba6b30088a3d).
Repeated revisions are combined into their current behavior rather than counted
as separate features. Website changes are listed separately below.

!!! warning "Alpha, not a stable release"
    These changes belong to `runeschema-7.8-alpha`, not the stable 0.7.7.9m
    release. The latest framework is installed and hash-verified on the test
    Steam and Game Pass clients. Automated checks do not establish that every
    new feature works in a live world or on a dedicated server. The final
    section identifies outstanding acceptance work.

## Runtime and loading

- Integrated the supplied Helpy/Imp source changes into the alpha runtime and desktop tool.
- Hardened startup sequencing, deferred asset preparation, readiness checks and bounded recovery instead of treating hook installation as proof that a loader succeeded.
- Reduced reliance on executable-specific signatures through reflected function, type, property and layout validation where supported; native-only hooks still require a compatible binding.
- Hardened Steam and Game Pass provider selection, wearable refresh and world-save loader checks. Unsupported bindings remain explicit failures, not guessed calls.
- Added weak-object identity and world-generation checks around deferred operations, cached references and streamed actors.
- Hardened shutdown and worker/window teardown so pending UI work is not treated as safe engine work after the world or runtime retires.
- Preserved the stock UE4SS boundary: RuneSchema packages do not replace or patch `UE4SS.dll`.

## Registries and cooked content

- Extended PAK registry declarations to recipes, journals and lore, with ordinary loader bodies embedded in cooked metadata where supported.
- Added conventional-root registry shards so supported declarations can be discovered without depending solely on Asset Registry enumeration.
- Added bounded, atomic declaration publication, owner/path checks and precedence for loose schema overrides of cooked loader metadata.
- Extended loose `/registry` declarations to eleven native lanes with explicit `Source: Schema` provenance and native identity checks.
- Fixed canonical network counting: already registered candidates are validated but no longer reserve new slots a second time.
- Kept the real 65,535-entry network capacity; genuine overflow reports existing, new and maximum counts instead of silently widening a native type.
- Removed unrelated 32,768-entry gates from item/recipe discovery and routing while retaining bounded paging, native validation and ambiguity rejection.
- Added typed metadata discovery for mounted equipment and recipe subclasses, including equipment whose asset name does not start with `ITEM_`.
- Deferred cleanup certification when required registry enrollment fails; a mounted PAK or successful asset load alone cannot certify a safe registry.

## Recipes and discovery

- Added convenient recipe-path overrides for automatic grants or supported native discovery conditions without requiring a separate raw progression patch.
- Added `Discovery` support for first-pickup history, actor interaction and skill-level prerequisites, with bounded native AND/OR groups.
- Preserved cooked progression with absent/`Native` discovery; rejected conflicting automatic and conditional policies rather than weakening a condition.
- Preserved already learned recipes. Automatic delivery checks existing native progress instead of treating each new session as a fresh unlock.
- Changed progression overrides to detach only the requested recipe, preserving unrelated recipes, buildings, achievements, conditions and native null slots.
- Added staging, native readback and rollback for rejected discovery transactions; changing discovery requires a full restart.
- Indexed canonical PersistenceID lookups once per batch, retained paths instead of stale raw pointers, and cached empty/failed lookups without accepting ambiguity.
- Added scoped weak-reference preparation and indexed crafting membership to reduce redundant catalogue work.
- Batched processing-table verification with ordered snapshots, append readback, deferred replay and stale-storage detection.
- Added phase/call counters to distinguish native asset loading, discovery, crafting placement and processing placement.

The measured Game Pass runs did **not** establish a meaningful overall recipe
startup speedup. Native loading and crafting placement remain profiling targets.
Discovery uses the game's native facts: item history is not a requirement to
retain or consume an item, and actor interaction is not station construction.

## Journals and lore

- Expanded authoring and overrides across native Lore, People, Place, Treasure, World and Recipe entry types.
- Added friendly World, Recipes and Knowledge routes plus exact native subcategory paths and supported group creation.
- Added validated native unlock-condition fields for item pickup, recipe discovery, interaction and AI kills; accepted the native trigger enum without inventing a synthetic region operand.
- Added NPC `JournalEntries` discovery on interaction while preserving the NPC's ordinary dialogue/shop behavior.
- Separated asset registration, category placement and actual player unlock confirmation.
- Corrected grouped-entry keys and hierarchy/detail lookup so list selection and the detail pane use the same native identity.
- Preserved earned unlocks and unread state instead of re-marking already unlocked entries on every launch.
- Allowed independently verified journal/lore unlock delivery even when an unrelated enrollment failure still blocks cleanup.
- Coalesced identical unlock failures from repeated inventory/combat callbacks without marking failed delivery complete or disabling retries.

Journal visibility does not itself grant a recipe, and building a station does
not implicitly unlock a journal. A supported native condition or explicit
gameplay trigger is still required when automatic unlock is disabled.

## Quests and events

- Expanded quest presentation for multiple independent quests, native categories, completed descriptions, completed-list visibility and objective hints/subobjective text.
- Added `Region` resolution through the live native enum, allowing future game-added enum names without a RuneSchema allowlist update.
- Separated presentation fingerprints from gameplay changes and added proven legacy migration for region/completed-description additions without replaying rewards.
- Added stage `Completion: Any` for alternative mandatory objectives sharing one quest/run/reward receipt; default `All` behavior remains intact.
- Added deterministic kill-versus-hand-in precedence and consumption of only the winning hand-in route.
- Preserved native counters, repeat cooldowns and pending-exchange safeguards against duplicate costs or rewards.
- Added item-side `$QuestScroll` contracts for accepting an existing quest through a craftable consumable.
- Included the neutral content-only `RuneSchema.QuestItems` plugin and editable/reproduction source, allowing cooked or permanent-cloned scroll items without a JobBoard-specific dependency.
- Validated the scripted native UseItem signature, owned main-inventory slot, authority and exact item identity before accepting a scroll use.
- Corrected an overstrict scroll check that rejected a valid native item while its nonreplicated save-identity cache was still empty; malformed populated identities remain rejected.
- Extended confirmed summon/helper kill attribution and callback integration while retaining authoritative ownership and native kill-credit checks.
- Added EventOnly AI additional-drop contracts with native loot-row binding, verification and restoration on non-kill retirement.
- Preserved completed event/quest receipts and cooldowns during eligible definition migration rather than fabricating completed stages or resetting the whole character.

Arbitrary custom region labels are not new native regions. The native enum must
contain the authored region. Quest-scroll consumption and alternative-objective
flows still need the live acceptance tests described below.

## World placement and NPCs

- Unified `$`, `$+offset` and `$-offset` ground selection across supported spawn, NPC, vendor and event placement paths.
- Made bare `$` prefer landscape beneath roofs/bridges; added `GroundReferenceZ` for a chosen stair landing, deck or upper floor.
- Rejected wall/underside hits and initial overlaps, and limited intended-floor selection to 45 cm above / 300 cm below its reference.
- Fixed trace endpoints so a distant lower surface cannot masquerade as the intended floor.
- Kept placement queued when streamed collision is unavailable instead of reporting an absent actor as placed.
- Corrected reflected World Partition cell-box decoding and inclusive cell boundaries used by NPC/spawn readiness.
- Added validated actor identity/lifetime checks after deferred construction and preview-class loading, with bounded retries only for eligible lifetime failures.
- Added scaled five-point capsule-footprint preflight for complete event waves, final-scale construction and collision-checked reconciliation of construction movement.
- Kept numeric-Z placement exact and retained post-construction position, collision, authority and save-exclusion checks.
- Hardened transient actor cleanup to require proven ownership rather than deleting a world actor merely because an identity or location matches.
- Added Human NPC wearable-slot validation, Necklace/Trinket2 handling and quest-completion equipment consequences through `EquipmentWhen`.
- Added explicit `WorldEverCompleted` equipment latches tied to the native world-save GUID, persisted in a RuneSchema sidecar and replicated by authored rule identity.

World-side NPC equipment ledgers must travel with world backups. These changes
do not claim that the reported missing silver vein, every NPC or every event
wave has passed a live streamed-world retest.

## Wearables and schema parity

- Added `$WearableBridge` schema directives for validated cooked or schema-created wearable enrollment.
- Enrolled schema-created wearables after successful native identity registration rather than treating cooked PAK content as the only supported source.
- Added dependency declaration examples for MaxCape and shared registry safeguards across schema/cooked lanes.
- Replaced repeated tracked-vector membership scans with weak-validated indexed lookup.
- Kept enrollment/readback diagnostics distinct from mere asset discovery; JSON does not create missing meshes, materials or Blueprint behavior.

The MaxCape test still reported the wrong visible appearance and
`NOT-ENROLLED`. Its schema contract is not a confirmed visual fix.

## Settings and mod management

- Switched shipped configuration to `settings.ini` while retaining legacy JSONC support.
- Added noninterrupting two-launch migration: translate/verify first, then back up and retire only the unchanged translated JSONC on a later launch.
- Preserved unknown settings, conflicting edits and recovery files instead of deleting uncertain configuration.
- Hardened saved enable/disable choices and load-order reconciliation so discovered/restored folders do not silently override an explicit disabled choice.
- Added complete Lua packages under `mods/<Owner>/ue4ss/<Package>/`, retaining Scripts, configs, metadata and dependencies in place.
- Added package-local helper/global context, callback context, duplicate-start prevention, collision checks, stale-manifest rejection and parent-mod enablement.
- Retained the legacy direct Lua companion layout; package changes require restart, not world reload.
- Added an opt-in, off-by-default corrupt-character entry bypass with diagnostics and fail-closed native-binding checks.
- Added carry-weight native readback and conflicting-rule diagnostics without rewriting user player profiles.

Nested native/hybrid DLL packages are explicitly rejected. Install native mods
through stock UE4SS; Lua namespace isolation is not a security sandbox or an
independent UE4SS VM/ModRef.

## Save safety and diagnostics

- Restricted persistence cleanup to proven unresolved/orphaned identities, preserving active learned recipes, journal progress and unrelated vanilla content.
- Strengthened startup registry-health and cleanup gates; failed enrollment blocks destructive cleanup instead of guessing that content was removed.
- Hardened Steam/Game Pass world-save preflight/provider boundaries; unsupported save formats or bindings remain protected.
- Hardened native notification text validation and separated successful delivery from merely scheduled notifications.
- Reduced routine PAK-registration, wearable discovery and repeated placement/unlock announcements.
- Added bounded advanced-log summaries with counts and content-root source hints while retaining genuine failures in normal logging.
- Moved engine-backed authoring actions behind readiness, session/config and per-action safety gates.

## Helpy desktop experience

- Integrated Helpy 2.0.4 work into the separate desktop tool; retained `RuneSchema.Helpy` IDs/state paths while introducing RuneSchema Modding Kit branding.
- Restored a normal movable desktop window, with F2 hide/reopen and game return, rather than embedding editing in the focused game window.
- Simplified Simple/Advanced presentation while keeping ordinary item giving available in Simple mode.
- Added responsive layout, adjustable Item Bay dimensions/page sizes, grid/row views and clearer selection/cart actions.
- Added real item drag/drop and right-click cart actions, with a shared cart and exact delivery confirmation before clearing delivered entries.
- Added kit/loadout import/export and local kit-library workflows, with explicit auto-equip support where native validation permits it.
- Added up to sixteen authoritative recipients for one cart, serialized delivery and stop-on-partial/ambiguous outcomes rather than automatic retries.
- Preserved hover descriptions/stats, right-click actions and a selected-item details pane in both docked and detached Item Bay views.
- Added a separate movable Item Bay with Dock back/close-to-dock, shared selection and cross-window drag/drop.
- Split item authoring into Transmog/Creation and Recipes workflows with visual source/customize/result and input/station/output layouts.
- Added appearance/icon drop targets, permanent-clone export to mod assets and prepared-result checks without changing the original item.
- Carried confirmed clone icon metadata through catalogue cards, cart rows, drag payloads and caches, including cross-content-mount appearances.
- Used native power-level/masterwork metadata for supported badges rather than guessing solely from item names.
- Made the logo's R track the chosen accent while keeping its brackets unchanged.
- Added environment marks, RuneSchema/executable version reporting and compact attribution/legal information without inventing a missing executable version.
- Moved advanced authoring into a dedicated Authoring Tools tab: schemas, captures, traces, profiles, diagnostics, presets, NPC import, effects and guarded offline cleanup.
- Added desktop startup support for dedicated servers with an interactive desktop; this is not a remote administration service or a renderer for a truly headless session.

## Helpy catalogues and icon cache

- Merged loaded objects, mounted metadata, cooked declarations and schema definitions into bounded item/recipe browsing.
- Deduplicated by object path, searched before paging and visibly gated unavailable definitions rather than presenting them as usable assets.
- Added typed crafting/processing station lookup, native labels/icons, exact recipe membership and station filtering; armor items, upgrades and vendor tables are not treated as stations.
- Prevented failed/empty mounted discovery from being recorded as a completed catalogue generation.
- Added cache-first AppData icon storage grouped by source/fingerprint, with native capture/save for misses.
- Compared content sources off the game thread and invalidated changed/missing sources independently; cache files cannot register absent mods or create items.
- Added registered item/building icon discovery outside conventional UI/Icons folders, including unloaded equipment candidates.
- Added optional `.rsmk-icons` import/export, source filtering, native file drops and local rekeying of validated compatible pixels.
- Added main-menu registered-icon cache building, staged progress/counters, cancellation, bounded capture and explicit partial-discovery reporting.
- Added Clean Icon Cache and Open cache folder controls with confined cleanup/junction checks.
- Kept capture demand-driven during normal startup instead of automatically loading/rendering every game asset.

Source grouping reflects known content roots/declarations, not guaranteed
physical PAK provenance for every virtual asset. Bulk capture needs a renderer;
it cannot capture icons in a genuinely non-rendering server session.

## Helpy map and building tools

- Added map travel using temporary World Partition destination preparation, native landing checks and the game's teleport/loading-screen route.
- Added travel phase/elapsed feedback and pre-departure cancellation; repeat travel skips unrelated placement lookups.
- Expanded the map to an aspect-correct widescreen workspace, cursor-anchored zoom up to 32×, pan and Fit world.
- Added debounced, world/request-correlated ground-Z lookups for hover and selection without occupying the gameplay mutation slot.
- Added a categorized building browser using native catalogue labels and building icons, with bounded metadata paging and UI-side filtering.
- Removed the requirement that a matching native building already exist in the world before first placement.
- Added concrete building-default/root validation, deferred binding, native collision rejection and post-construction verification.
- Distinguished live placement, saved-but-inactive placement and failure; temporary placement remains transient/save-excluded.
- Added read-only live player role badges for roster/map presentation, with authoritative snapshots, expiry and stale-reply fences.

Building is an admin/authoring route, not an embedded player hammer menu with
material costs and snapping. Role badges are not authorization tokens and do
not implement grant/revoke/kick administration.

## Removed and superseded behavior

- Retired the custom UE4SS-host/native nested-package experiment; build and bundle entry points reject it, and original client UE4SS hosts were restored.
- Removed obsolete host API assumptions from the current RuneSchema runtime; historical fixtures are not a supported installation route.
- Removed obsolete engine-embedded authoring UI in favor of the desktop Authoring Tools tab.
- Replaced superseded, overly rigid contract tests/references with current runtime, schema and lifecycle regression coverage; retained active safety tests.
- Removed blanket journal ownership rewriting and repeated unread/unlock behavior for already earned entries.
- Removed per-object PAK/wearable success spam in favor of bounded advanced summaries.
- Removed artificial 32,768-item/recipe catalogue rejection and duplicate reservation of already registered network identities.
- Superseded the runtime-only quest-scroll prototype with the verified shared content helper and its stricter use contract.

## Website and documentation

- Simplified top-level navigation into Players, Creators, Servers, Reference and Updates, with collapsible reference groups.
- Reworked the home page into a quieter responsive layout with one primary action, clear stable/alpha labels and three audience routes.
- Added quick-guide filtering, aliases, result counts, Clear, Escape reset and Enter-to-open behavior.
- Preserved keyboard focus indicators, light/dark themes, reduced-motion behavior, site search and existing guide URLs.
- Updated loader schemas, authoring references, examples, platform guidance and candidate verification records alongside the runtime changes.
- Added this consolidated alpha changelog instead of presenting chronological candidate receipts as a finished release announcement.

## Verification and remaining work

The latest installed candidate passed **184 core tests, 35 Helpy tests and 75
generated-schema checks**, native builds, canonical UPX integrity and **269 core
imports against each stock Steam/Game Pass UE4SS host**. Installed framework
hashes and the Game Pass launch alias were verified. The
[candidate record](https://github.com/gh0sted5456-us/RuneSchema/blob/runeschema-7.8-alpha/docs/ALPHA-CANDIDATE.md)
contains package hashes and per-candidate evidence; older installation labels
there are historical, not separate current recommendations.

Still outstanding:

- Live streamed-cell placement of the reported silver vein/NPCs and full scaled event waves, including save/reload and retirement.
- First temporary/permanent Iron Chest placement and permanent reload behavior in a real world.
- Main-menu/full icon capture, mod/clone icons and complete station/catalogue coverage on both clients.
- Quest-scroll use, crafted/reloaded identity, inherited CanUse, alternative hand-ins, repeat/reconnect and multiplayer receipts.
- NPC world-equipment persistence, late join, cell reload and dedicated restart.
- Journal detail/scrolling/unlock and existing-character recipe-discovery acceptance in current content.
- Distant/repeated travel and dedicated/co-op presentation, role badges and safe rejection.
- MaxCape's unresolved visual enrollment and the reported shrine/idle/exit crashes; hardening is not proof of a universal crash fix.
- Further measured startup and large-station UI profiling. Consumable pattern packs are mod-content work, not a RuneSchema optimization already delivered.
- Server Admin grant/revoke/kick controls, native account-owner mapping and target-aware remote mutation permissions.
- Per-fuel processing-speed schema: native speed functions were found, but fuel identity, transitions, persistence and buff composition remain unproven and unimplemented.

Content authored by COLORS, Shadowmancer and JobBoard remains independently
owned. Framework support does not imply that every content draft was installed,
recooked or validated. No stable promotion or release tag is part of this alpha
update.
