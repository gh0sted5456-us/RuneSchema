# RuneSchema and RSMK 0.8.0.1 changelog

Draft for the next `main` commit, consolidating verified work after the published
0.8.0.0 release. This document does not mark 0.8.0.1 as published or change the
runtime version. Repeated development revisions are summarized once below.

## Helpy Home and presentation

- Added a Home landing page with tiles for the workspaces enabled in Settings.
- Collapsed navigation on Home and restored it when entering a workspace.
- Added hover descriptions to Home tiles and quick-access settings/cache controls.
- Consolidated Info, Credits and Legal into one Runtime details window.
- Moved the runtime icon to the far right of the workspace heading.
- Added Runtime, Attributions and Legal tabs with RuneSchema, UE4SS, Helpy/RSMK and game-version information.
- Added a centered splash alternating Dragonwilds and RuneSchema branding, with four-second holds and four-second eased crossfades.
- Preserved Enter throughout the splash instead of requiring a complete animation cycle.
- Added the selected-logo exit: RuneSchema brackets move outward; the Dragonwilds wordmark separates vertically; both fade away.
- Respected Windows reduced-motion preferences and kept branding readable without animation.
- Applied the selected accent color to RuneSchema's R during holds, blends and exits while retaining neutral brackets.
- Added centered rotating tips and a centered optional Download Initial Assets prompt.
- Added persistent Hide this prompt with a confirmation directing users to Settings > Updates & runtime > Initial assets.
- Kept RuneSchema and Dragonwilds logos bundled so splash branding works offline from first launch.
- Separated optional UI artwork from the lean runtime package and added pinned-size/SHA-256 verification for its explicit GitHub download.
- Retained existing downloaded artwork on failure and skipped unchanged files during repair.
- Added configurable picture backgrounds with previews, subdued theme overlays and a clear-glass alternative.
- Made picture-backed Helpy windows opaque behind the artwork so unrelated desktop windows do not show through.
- Added optional downloadable background artwork with RuneScape/Jagex attribution.
- Removed the Home community banner and retained the larger centered RuneSchema logo over the chosen background.
- Added unlabeled GitHub, Nexus Mods, CurseForge and Discord footer icons with direct RuneSchema/community links.
- Centered destination detail cards, constrained them to the viewport and added static page-preview artwork.
- Moved the complete community banner into Discord's detail card and removed management controls from the supplied Nexus/CurseForge previews.
- Removed residual hover-card focus after navigation so old cards do not cover later controls.

## Windows, layout and workspace controls

- Added taskbar minimization and independent restoration for detachable Helpy tools.
- Added minimize, maximize/restore and close controls to Item Bay, Icon Cache, Map, Recipes and Runtime details windows.
- Placed docked tool controls beside the title-bar X instead of duplicating them in content rows.
- Preserved tool position, size, selected runtime tab and editor state through detach, dock, minimize and restore.
- Limited docked maximization to the active Helpy surface rather than the combined desktop canvas.
- Added Expand pane / Restore pane for the Item Bay while retaining selections and editor drafts.
- Wrapped toolbar actions at narrow widths and refined splitters and responsive layouts.
- Centered recipe headings, station artwork/names, Grid/Rows controls and empty input/output prompts.
- Added saved workspace visibility controls for advanced tools, Live Log, Save Editor and Server / Co-op.
- Added Server / Co-op access for ordinary client hosts without treating co-op as DedicatedServer.ini hosting.
- Removed the obsolete F7 Canvas Helpy renderer, input adapters and Canvas-only test harnesses; F2 desktop Helpy remains.

## Item loading, search and attribution

- Added source/locale-scoped indexed catalogue pages so cached searches and the first alphabetical page do not require restoring the entire catalogue.
- Moved catalogue file reads, filtering, sorting and source comparisons off the UI/game thread where supported.
- Preserved last-good rows while the active query is checked or refreshed in the background.
- Staged replacement batches until they cover the prior visible prefix, or a definitive shorter result is available.
- Preserved page position, fill targets and already-loaded pages during same-query background refreshes.
- Prevented incomplete verification slices from appearing as a final zero-item result.
- Removed the source-removal latch that disabled Item Service during mod updater remove/replace sequences.
- Marked cached query windows stale and revalidated them at low priority without clearing usable results.
- Retained request, cancellation and world-generation fences so late replies cannot replace newer searches.
- Refreshed mounted ItemData metadata when verified content scope changes, not only when the world changes.
- Separated icon-dependency changes from actual content changes to avoid repeated catalogue rebuilds.
- Added a fixed toolbar loading-wheel slot so refreshes do not repeatedly resize the Item Bay.
- Moved detailed loading stages, cache coverage and warnings into wheel hover details.
- Hid names beginning with [PH] by default while retaining an author opt-in and the underlying cached records.
- Corrected official/native content attribution using bounded plugin-descriptor metadata from known native base packages.
- Fixed the Dried Tea Leaves native-source case and stopped treating borrowed mount/icon references as proof of mod ownership.
- Preserved exact clone creator evidence and repaired cached attribution before source-filter matching.
- Reserved the top-right RuneSchema badge for genuinely mod-created/mod-supplied content.
- Added a separate bottom-left edit indicator for native items modified through confirmed RuneSchema edits.
- Added last-recorded modifier details without claiming a complete patch history.
- Kept USMAP schema decoding separate from physical package-ownership evidence.

## Icon caches and initial assets

- Kept writable Helpy caches in RuneSchema AppData rather than inside mod packages.
- Added automatic import of matching active-source cache/*.rsmk-icons packs supplied by mods without modifying those packs or registering items.
- Preserved resident icons, queued work and retry state for unchanged sources during unrelated mod updates.
- Rechecked disk immediately before capture so newly imported icons do not require another native render.
- Excluded cache/export directories from content fingerprints so writing a cache does not invalidate the content it describes.
- Moved recursive cache maintenance and cleanup to an owned filesystem worker.
- Added bounded icon pumping and separate timing attribution for asset loading, drawing, GPU readback, cache reads and cache writes.
- Simplified cache tools to source selection, Import, Export and Build / update, with advanced details available separately.
- Retained explicit incomplete-coverage warnings instead of claiming a partial export is complete.
- Published a Steam CL-251392 base-game preview pack with 4,271 captured and verified icons and zero capture failures.
- Added explicit base-game cache download with runtime/build candidate matching and strict portable content-identity verification before import.
- Verified repeat import reports matching icons unchanged instead of rewriting them.
- Published the loose optional UI artwork separately from the runtime package, retaining ownership and attribution notices.
- Renamed the combined artwork/map setup action to Download Initial Assets; game item-icon packs remain separate.

## Map resources and runtime information

- Added an optional cached Wiki/Wildsforge reference layer with searchable resource/location groups and individual type toggles.
- Added Show all, Hide all and overall cached-layer visibility controls.
- Persisted cached category choices, native marker toggles and the map-update preference locally.
- Kept reference locations distinct from loaded game markers and authored RuneSchema placements.
- Grouped nearby reference markers at low zoom to bound draw work without discarding indexed locations.
- Added visible source/licence credit and retained CC BY-NC-SA 3.0 attribution in the downloaded cache and legal notice.
- Kept the native game map artwork/projection and existing ground/authority checks for map actions.
- Limited online reference-data refresh to confirmed installed Dominion build changes, separately scoped by storefront.
- Prevented dates, website announcements, mod changes and RuneSchema/UE4SS updates from triggering map downloads.
- Kept map data and selections available during update failures, and rejected malformed, empty or oversized replacements.
- Added map Refresh wheel details covering loaded-area scans, cache status and installed game identity.
- Added native connection-role freshness, known player count and runtime responsiveness to Runtime details.
- Added a co-op host overlay to storefront badges only while a fresh listen-server observation supports it.
- Cleared stale connection-role observations on timeout or world changes without claiming measured RTT or packet loss.
- Added bounded native startup checkpoints and selectable human-readable time units; these measure RuneSchema's pipeline, not total game/world loading.

## Cart validation and delivery

- Added a bounded read-only validator for staged cart paths instead of a full catalogue scan.
- Added a green item check for current-runtime ItemData/publication evidence, separate from delivery permission and quantity checks.
- Invalidated cart validation when source/world scope changes and ignored stale validation replies.
- Allowed definite pre-inventory rejection of one item to continue to other items and recipients.
- Kept unknown or unconfirmed inventory outcomes behind review without automatic retry or rollback.
- Removed confirmed/rejected entries from the active cart while retaining bounded green/red/amber outcome placards and per-player receipts.
- Kept cart cleanup separate from inventory, save and asset deletion.

## Save Editor, logging and safety

- Added readable Save Editor inventory pages with resolved item names/icons/source, PersistenceID, instance ID, saved slot, stack count and equipped state where current native metadata is available.
- Added page-local name/ID/source filtering and selection of up to 128 records across pages.
- Added reviewed draft-only removal while preserving sparse slots and unlinking known native equipment references.
- Added guarded Count edits; stack increases require a verified native MaxStackSize.
- Retained originals and unknown fields and exported reviewed edits as a new verified save copy.
- Made RuneSchema.log a continuous current-run log independent of Helpy's bounded viewer.
- Retained RuneSchema's normal messages in UE4SS while keeping advanced detail in the dedicated log.
- Added an opt-in restart-required log archive setting with UTC datetime filenames and no automatic archive deletion.
- Split Live Log into RuneSchema and UE4SS tabs with independent search, pause/follow and view controls.
- Consolidated file/folder access and filtered export into Actions while keeping filters/colors available without a verbose default layout.
- Prevented log export from overwriting active logs or RuneSchema archives.
- Made uncertified persistence registries defer optional cleanup without blocking native character loading or modifying saves.
- Preserved independent verification of usable content instead of treating one incomplete registry as a global content failure.
- Retained the Abyss (RSDW Modding Community) credit for the initial corrupt-save bypass work.

## Quests, lore and wearables

- Retained the separately integrated Quest Board encounter hooks without bundling optional starter-quest content in the framework ZIP.
- Added automatic Quest Cancellations items/recipes for registered acceptance scrolls crafted on the supported Quest Board contract.
- Added Action: Abandon while preserving Accept as the default quest-scroll action.
- Consumed one cancellation scroll only after verified quest reset; inactive/completed/pending or uncertain cancellations do not consume it.
- Cleared the matching active/staged quest state and that character's matching encounter without issuing rewards or refunds.
- Preserved paid completion history and reset staged-run state so cancellation/retry cannot restore first-run rewards.
- Added generated cancellation identities that do not recursively create more cancellation scrolls.
- Routed viewed lore through the validated native journal read operation instead of inventing local unread/open flags.
- Retained lore open/glow presentation driven by actual native read state; live re-entry/co-op acceptance remains outstanding.
- Accepted legitimate wearables with empty material-hide sections without requiring a nonexistent native hide-cache entry.
- Retained native enrollment validation and diagnostics for wearables with nonempty hide lists.

## Verification and release boundaries

### Source additions — r34–r36 (r36 installed; not published)

- Added local character-ID-bound portrait choices from currently cached game/mod icons and optional Wiki artwork. No player name matching or claim of automatic co-op avatar distribution.
- Added party gear and skill strips, persisted Advanced display toggles, optional repeated hover vitals/technical explanations, and a separately detachable/minimizable Party window.
- Added checked native read-only loadout inventory and SkillData/GetSkillLevel presentation. Live/co-op gear and skill synchronization acceptance remains outstanding.
- Added a boundary-connected ocean mask from the native map artwork to the teleport advisory. Ocean cells remain red even if a footprint check finds a surface; this is cartographic guidance, not a swimming/terrain guarantee.
- Added selectable resource details with separate fresh runtime availability and cached reference/Wiki information. No fabricated respawn deadlines.
- Redesigned Spawns around choose/place/review, actual native enemy-character verification, bounded relative portrait scaling, and original player/tree/stone/horned-axe silhouettes as fallbacks.
- Added an explicitly downloaded, local Wiki artwork/article cache with demand-loaded textures and separate Jagex artwork/Wiki-text attribution. It is optional reference material, not the native journal or character progress.
- Added a native-first Journal/Lore workspace with source filters, persistence IDs, multi-page reading and native artwork references. A small read-only runtime slice never unlocks entries, marks books read or modifies saves.
- Added asynchronous AppData journal-cache loading/writes, a separate observed/unmodified vanilla cache and support for an optional supplied assets/journal/vanilla.json. Cached references remain browsable during scans and are explicitly distinguished from entries observed in the current runtime.
- Added an Author tab for new journal/lore drafts, multi-page editing and native text/art templates. Export uses new IDs, defaults Unlock to false, validates cooked image/category references structurally and does not install or overwrite a mod.
- Reused the splash's outward, fading bracket animation for Home workspace-tile selection before navigation. Duplicate tile activation is suppressed and Windows reduced-motion preferences skip the transition.
- Full vanilla journal coverage, native journal artwork/text capture in the disposable world, co-op portrait sharing and current equipment/skill behavior still require live acceptance. A completed loaded-object scan is not proof of a complete vanilla cache.

### Installed development candidate — r37

- Centered equipped strips, including wrapped lines, and added a thin gold border around each complete party-member card.
- Added World, Recipes and Knowledge journal selectors. All entries retains unclassified references; categories are not guessed from titles.
- Fixed the portrait modal rendering outside Helpy's theme scope. Its sources now include Item Bay rows, resident textures, native journal artwork, optional Wiki pictures and original SVG portraits, with display-name search.
- Added original player, tree, stone/ore and horned axe-bearing enemy SVGs. Scale previews share their canonical vector paths.
- Fixed read-only spawn-presentation artwork lookup rejecting loaded class-default objects. Native localized display names take priority; resource-state labels are cleaned for presentation without changing underlying class keys.
- Added ambiguity-aware native-journal artwork matching to spawn rows and scale previews.
- Added View inventory and an independently detachable, minimizable inventory surface. Host/server reads bag or personal-inventory data in bounded 32-slot pages with current character/world guards. Refresh is read-only; adding uses the existing reviewed Item Service delivery flow.
- Direct deletion, direct personal-chest edits, remote-admin inventory access and live personal-inventory persistence acceptance remain unfinished. No destructive inventory operation is enabled.
- Added a saved, opt-in placed-chest/storage map filter using loaded building actors with a unique owned native inventory component, not a name substring. LMB opens the detachable contents inspector with a chest icon, native item icons, stack counts, paging and refresh.
- Fenced placed-storage reads by authoritative world generation, weak object index/serial, exact actor path and current component ownership. Streamed-out, replaced or ambiguous storage is unavailable, not an empty chest. Reads use reflected native contracts rather than platform-specific memory offsets; Steam/Game Pass live acceptance remains pending.
- Chest lock/owner readings remain explicitly unknown. Direct chest deposits and override unlock remain disabled pending verified per-chest access and persistence contracts; player default-privacy settings and network ownership are not lock evidence.
- Corrected original portrait rendering to use its own cached vector geometry rather than falling back to the map-pin glyph.
- Linked the native core and Helpy DLLs and passed 55/55 UI tests, including full/compact/minimum render fixtures, map-chest LMB selection, selected-container receipts, detached inventory close/minimize/restore, original portrait geometry and saved chest-filter round trips. Visually checked the portrait picker, wrapped equipped strip and chest inspector.
- Installed r37 on Steam and Game Pass after confirming both were closed. Replaced four DLLs total and added four original portrait SVGs per installation, with verified backups, matching Game Pass launch-mirror hashes and 47 protected configuration/dependency files unchanged. No game launch, settings/cache/save writes, version bump or publication was performed. Direct chest deposits and unlock overrides remain disabled.
- Read both bundled platform schemas offline. Completed an isolated Steam menu-only metadata probe without entering a world or invoking equip, deposit or unlock operations. Confirmed native linked-inventory lock setters exist, not their safe chest-access semantics. The isolated Game Pass probe was blocked by protected-executable copy access, so live Game Pass storage behavior is not certified.

### Current source candidate — r33 (not installed or published)

- Added opt-in **Equip on receipt** to the shared item/loadout cart. The intent is frozen per delivery batch and defaults off.
- Kept equipment status separate from confirmed inventory delivery. Failed, skipped or uncertain equipping never repeats a grant or reclassifies an already confirmed inventory delta.
- Added checked native inventory-instance snapshots, GUID identity matching, native eligible-slot lookup and empty-slot-only transfer. Ambiguous instances, existing gear, changed ownership/world/character and changed reflected contracts stop the equipment step safely. Non-equipment items are never consumed with a generic UseItem call.
- Added exact-instance readback after the one native transfer; a native acceptance flag alone is not success.
- Added native hunger, thirst and rest/endurance readings from validated Sustenance, Hydration and Endurance components. Health, stamina and special power remain independent; food-effect occupancy and sleeping/resting state are not substituted for survival percentages. Missing, changed or stale contracts show unavailable, never fabricated values.
- Moved the host/house indicator beside the player name instead of over the health tracks. Paired the six meters in compact cards while retaining a narrow-width fallback and native power-level artwork.
- Added a versioned, whitelisted party-stat envelope to the authenticated co-op roster, with independent observation ages for all supported meters, power level, food effects and rest state.
- Converted host observation ages to the receiving clock and conservatively included request round-trip time; stale responses, unknown versions and malformed values are discarded. Existing connection, request-revision and world-generation fences remain required.
- Merged server stats into map rows only by unique current character identity; never replaced live positions, action handles or permissions. Remote client-local values are not labeled server-authoritative.
- Reused the coalesced read-only roster poll rather than competing with the mutation mailbox. Resource proximity now uses actual map-player positions, including replicated client pawns; managed-marker refreshes retain their measured resource observations.
- Added a teleport-selection advisory overlay: yellow marks unchecked space, green marks a recent successful native landing-footprint check and red marks a measured slope/ledge issue. Unknown or unloaded terrain is not classified as safe. Checked points expire, follow the selected player's identity and never disable an attempt; final native travel checks remain in force.
- Added portable identity/readback tests, clock-skew/expiry/ambiguity tests, separate equip-failure delivery tests, advisory-risk expiry tests and four core-header parity checks. Current verification: 202 core tests, 54 Helpy tests and both native DLLs linked, including compact/minimum-size rendering.
- Captured nested native resource respawn settings and duration metadata in an isolated menu probe. Native game-time selection, player-count reductions and distance/pause rules mean a fixed countdown is not proof of respawn; exact deadlines remain unaccepted.
- Live acceptance is in progress in an explicitly authorized, isolated Steam test profile (`RSR33Test` / `RSR33Tester`). Existing installed worlds/characters were not opened. This is not a release or multiplayer/Game Pass acceptance claim.
- In that isolated Steam world, the user confirmed all six meters are visible. A direct UI inspection also confirmed the house badge beside the player name, the party context menu beside its clicked row and the yellow unknown-terrain advisory during teleport selection. Exact meter/HUD agreement, green/red terrain outcomes and live co-op remain unaccepted.
- The first isolated equipment attempt confirmed delivery but safely skipped equipping before any transfer because the UE4SS object-property setter was unsupported. The checked reflected argument write has been corrected; successful equip/readback acceptance remains pending. No uncertain delivery was retried automatically.

### Current candidate work — r32 installed on Steam and Game Pass

- Created 64 original, theme-tinted vector glyphs for map layers, resources, party stats, equipment and common actions; exported individual SVGs and a review gallery.
- Reused one canonical path set for SVG export and the native renderer. Geometry is parsed once, not for every marker/frame; no artwork download is required.
- Added glyphs to cached map filters and clustered markers. Mixed-category clusters retain a neutral grouped symbol rather than claiming one resource type.
- Added health, stamina and special-charge tracks to the shared party/recipient card; reused the game's native power-level badge, with a text fallback if its texture is unavailable.
- Added native no-input stat getters with reflected return-type/frame checks, finite value checks and observation timestamps. Missing or stale stats remain unavailable, not zero.
- Distinguished native food-effect slot occupancy and sleep/rest state from unverified hunger/thirst percentage gauges.
- Added persisted nearby-resource observation controls, including a 25–2000 metre radius around connected players. Sampling requires loaded resource visibility, is round-robin and has a 24-node/1 ms per-pass budget.
- Added measured loaded-node availability/quantity and native paused-state details. Exact respawn deadlines are not yet verified; no fixed or guessed countdown is displayed.
- Gave wrapped details tooltips an explicit viewport-bounded width and height cap, including Save Editor asset paths, loading details, recipe stations and spawn details.
- Stabilized map options dimensions and anchored party context menus to the clicked row, rather than the top of the map.
- Extended the candidate party-travel selector to another connected member or a selected map point, preserving confirmation, current character/world identity checks and safe landing checks.
- Captured current Steam menu-only metadata for stat/equipment/resource APIs without invoking game actions; separately scanned the running protected Game Pass executable read-only for relevant symbols. Symbol presence alone is not full Game Pass ABI or gameplay verification.
- Installed the verified r32 core and Helpy DLLs on Steam and Game Pass, with matching Game Pass launch-path mirrors and verified backups. Only four DLLs changed; 47 protected configuration/dependency files remained unchanged. Both games were closed for installation; no world or save edits were performed.

- Verified the current shared native build with 202 core tests and the current Helpy candidate with 50 tests, including normal, compact and minimum desktop render fixtures, 120-frame popup stability checks and 160-frame bounded-tooltip checks.
- Verified compressed DLL integrity, the compressed Helpy lifecycle, both mandatory splash logos and the 36-file lean candidate.
- Verified actual UI/cache downloads and map-marker parsing in isolated AppData, including unchanged-build refresh rejection.
- Installed r32 on Steam and Game Pass, including the original glyphs, current stat readers, bounded details panels and nearby-resource observations. Auto-equip, full authoritative co-op stat transport and verified respawn countdowns remain unfinished.
- Updated only the intended Steam core/Helpy DLLs for r30, with verified backups; stock UE4SS, user settings, optional mods, caches and saves were preserved.
- No 0.8.0.1 version bump, main commit, tag or release publication has been performed yet.

### Not claimed as completed

- Game Pass save attachment/conversion and general world-save editing remain disabled or unverified.
- The published base-game icon pack is Steam-specific; it is not a Game Pass cache or every game/mod texture.
- Automated and isolated tests do not replace live-world, reconnect, dedicated-server or multiplayer acceptance.
- Full authoritative co-op stat synchronization is not yet live-accepted: r33 adds the bounded authenticated stat transport, while installed r32 lacks it. Remote local/replicated readings must not be represented as proven server-authoritative values.
- Optional content mods and Shadowmancer's pak-only armor bonuses are not framework features or bundled release content.
- Original map filter glyphs and nearby-node radius controls are implemented in the candidate. Co-op-shared player-uploaded icons and verified harvested-state/respawn countdowns remain follow-ups, not shipped features.
- The movable translucent party overlay and left-select/right-context refinement are present in installed r32: it shows player names/icons and offers Locate, Teleport to and Recall to me with confirmation without taking space away from the map. Expanded destination selection and advisory risk colors belong to the uninstalled r33 candidate; live co-op travel remains unaccepted.
- Added a default player glyph and a cached icon slot for each party row; player-uploaded avatar import/co-op distribution remains pending, not implemented by the glyph.
- Reused the same player-card presentation for the map party and item-recipient selection, with optional fresh current/max HP; missing, stale or changed native health contracts show unavailable, never fabricated health.
- Current Steam inventory-transfer/loadout signatures are verified, including native slot lookup and item identity getters. Installed r32 has no auto-equip. The r33 opt-in exact-instance integration still requires live behavior/readback acceptance; signature discovery alone does not prove equipping.
- Expanded the metadata-only equipment inspector to report inventory-transfer/loadout candidates, native function flags, parameter layouts and component fields; it never invokes equip, use or transfer operations.
- The installed party-travel shortcuts require a local host character. The newer candidate also supports connected-member and map-point destinations without a local character; a dedicated server still has no "me" destination. Client badges alone do not grant teleport authority.
- Party travel resolves current controller identities, character GUIDs, world generation and destination position at execution, then uses the existing safe native travel path. Live co-op acceptance is still required.
- The earlier party-overlay revision passed 202 core tests and 49 Helpy tests. The current r33 candidate supersedes that result with 202 core tests and 54 Helpy tests, native DLL linking and normal/compact/minimum render fixtures. Live meter availability, co-op travel and equipment behavior are not proven by automated checks.

## Experimental checkpoint — 2026-10-18

Consolidated the installed r37 development candidate for the `experimental`
branch with the current published baseline. The native DLLs link, all 202 core
tests pass with an isolated writable test-temp directory, and all 55 Helpy tests
pass, including normal, compact and minimum render fixtures. No private probes,
installed-game backups or generated build trees belong in the source commit.
The runtime remains 0.8.0.0; this is not a 0.8.0.1 release or a claim that the
live-acceptance limitations above are resolved.

## Next main release

Prepare one reviewed 0.8.0.1 change set against the actual current GitHub `main`,
including complete new files and intentionally removed legacy Canvas files.
Exclude private probes, generated build trees, installed-game backups and unrelated
mod content. Update this changelog and rerun verification after the pending map
work before changing version metadata or publishing.
