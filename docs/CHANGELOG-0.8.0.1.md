# RuneSchema and Helpy 0.8.0.1 — line-by-line changelog

Cumulative changes after **0.8.0.0**, consolidated for the **October 23, 2026**
0.8.0.1 package refresh. Development revisions are combined, not presented as
separate releases. Each bullet describes one implemented change or retirement.

[Release/package identity](RELEASE-0.8.0.1.md) · [Download/status](CURRENT-RELEASE.md)
· [0.8.0.0 baseline](CHANGELOG-0.8.0.0.md)

## Implemented and improved

### Helpy window, Home and navigation

- Grouped Items, Clone, Recipes and the optional Character Editor under Players.
- Grouped Map, Spawns and the optional Journal under World.
- Remembered each navigation group's selected workflow without discarding drafts.
- Added a Home landing page with tiles for enabled workspaces.
- Added themed splash transitions, rotating tips and reduced-motion handling.
- Blended the main window background into an icon-only caption while retaining an accessible application title.
- Restored visible minimize, maximize/restore and close glyphs at the native right-hand caption targets.
- Retained native dragging, resizing, system-menu and window-control behavior.
- Centered the splash platform badge below the tips.
- Centered Home's runtime/status action above the social links and removed its duplicate header badge.
- Added a sidebar divider with matching geometry and persisted drag order across both navigation groups.
- Defaulted Settings to the upper navigation and Home, Profile and Report a bug below the divider.
- Kept narrow navigation groups scrollable rather than letting rows overlap the footer.
- Moved Open Helpy at startup directly into Quick settings, outside Advanced.
- Kept F2 available when automatic Helpy opening is disabled.
- Added themed drag previews and destination outlines for supported navigation, item, cart and mod drags.
- Preserved detached workspace state through docking, minimization and restoration.
- Added Item Bay Expand/Restore controls and improved compact toolbar wrapping.
- Centered shared details text while preserving readable tables and editable controls.
- Clamped hover panes to the viewport and corrected wrap widths that clipped long tooltips.
- Centered the map Spawn/Build/Teleport action window within the active Helpy viewport.

### Profiles, inventory presentation and mod management

- Added shared AppData profiles, portraits, loadouts, labels and spawn-browser preferences.
- Linked local character copies by native GUID and platform rather than display name alone.
- Added observed platform-account names, including the initialized Steam persona when available.
- Kept account display names separate from player authorization and remote-player identity.
- Added platform filtering and conflict-safe explicit profile saves.
- Added built-in Favorite/Junk marks with native inventory controls and stack/type scope.
- Preserved Favorite/Junk marks across manual storage moves.
- Added grouped item organization, grant-quantity and advanced-refresh context actions.
- Unified live inventory and Character Editor item placards, details and Item Bay controls.
- Preserved sparse saved slots, unknown fields and guarded draft-only count/removal edits in exported character copies.
- Added slot-compatible loadout pickers without treating an editor selection as a live equip action.
- Added native skill-icon/color presentation and verified XP-curve-backed editor level staging.
- Added readable equipped-item durability meters and unknown/stale-state presentation.
- Added stable local Hide/Restore preferences to resource, NPC and enemy browsers.
- Rendered disabled mods with black, unnumbered power-level icons.
- Numbered only enabled mods consecutively through filtering and reordering.
- Added source/mod-friendly names from the same metadata used by the mod list.
- Added individual provider filters using stable owner tokens rather than display names or PAK-name guesses.
- Added runtime-local mod-health history and excluded unrelated process logs from attribution.

### AppData, mappings and source-aware caching

- Consolidated shared Helpy/profile/cache data under `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core`.
- Separated Steam, Epic, Game Pass and server runtime state beneath the same AppData root.
- Created missing managed directories without moving or merging older runtime roots at startup.
- Kept recognized existing flat AppData folders in place when their core counterpart is absent.
- Stopped using the old install-local `runtime/live` tree for the current managed storage layout.
- Moved generated mappings into platform-specific AppData caches instead of shipping `dlls/mappings`.
- Keyed mapping compatibility on the game executable/version and native content inputs, not RuneSchema/mod/load-order changes.
- Reused a validated mapping until game inputs change or the generated cache is missing/invalid.
- Validated replacement mappings before publication and retired only previously recorded generated files.
- Logged first-launch capture/next-launch reuse rather than pretending a missing mapping is already available.
- Supported the native Game Pass WindowsApps executable junction without allowing redirects inside managed data.
- Added container-first ownership discovery for mounted-content candidates, including PAK-only mods and native plugins.
- Added bounded legacy PAK v3 and modern PAK/IoStore directory handling with index validation.
- Recognized the game's validated native `global.utoc` as metadata-only instead of a broken mod container.
- Preserved ambiguous or unavailable source ownership instead of guessing a supplying mod.
- Added revision-bound ownership pages rather than broadcasting entire package maps in each heartbeat.
- Added version-2 `.rsmk-cache` bundles for typed presentation metadata, source routing and artwork.
- Retained legacy `.rsmk-icons` import compatibility.
- Added atomic, owner/category-scoped cache merges that retain last-good indexes on cancellation/failure.
- Added packaged-mod cache imports and explicit sharing of a selected mod cache through core.
- Fixed oversized icon-source replies by using the bounded response-size probe instead of a fixed 4 KiB buffer.
- Allowed bounded native icon capture from the menu as well as a loaded world.
- Moved thumbnail reads/indexing off the UI thread and bounded completed texture uploads.
- Kept artwork/cache references separate from native registration, delivery permission and save-cleanup authority.

### Runtime coexistence and performance

- Rejected unrelated bridge calls by cached function names before expensive class/path work.
- Cached bridge/class matching and narrowed observer work to relevant registered behavior.
- Deferred nonessential startup/catalogue work rather than forcing a complete UI scan before game loading.
- Added bounded work slices, cached queries and stale-generation checks across catalogue/tool requests.
- Kept icon work demand-driven instead of rendering every registered asset at startup.
- Reduced hidden/minimized/background UI and diagnostic activity.
- Updated DPI/frame metrics from window events instead of recurring DPI polling.
- Moved structured diagnostic serialization, coalescing and rotation to a bounded writer queue.
- Added measured phase/startup checkpoints without claiming they measure total Dragonwilds loading time.
- Preserved the stock UE4SS host and independent optional-plugin failure boundaries.
- Added verified missing `FObjectPtrProperty` value-size initialization using the host's discovered pointer-width reference size.
- Left existing host metadata, unknown layouts and unsupported hosts untouched by that compatibility repair.

### Registrations, recipes, quests and saves

- Added opt-in cooked native registry lanes for container contents, plants, trees/growth, notifications and player/world hooks.
- Validated exact cooked object paths, native types, aliases, instance readiness and network round trips before enrollment.
- Preserved existing network IDs as the registered prefix and avoided double-counting already registered objects.
- Reused the existing bounded RSREG shard path for large owner-scoped declarations.
- Added read-only verification for mod-owned cooked enums without extending native game enums.
- Added a bounded, owner-channel general mod-data relay and Lua helper using the shared bridge transport.
- Kept relay receipts distinct from proof of gameplay execution or successful state application.
- Preserved authored `Unlock:true` recipes as learned character progress and removed only their legacy transient-save exclusion.
- Kept recipe discovery overrides separate from unconditional unlock and station placement.
- Added Quest Board cancellation-scroll contracts with native state checks and one-item consumption only after verified reset.
- Added cancellation bookkeeping that preserves paid completion history and does not replay rewards.
- Added the Quest Board encounter activation-health correction without bundling optional quest-provider content.
- Pruned unfinished RuneSchema-owned orphan quests after hydration when the applicable registry and backup are verified.
- Preserved completed native/history records during automatic orphan cleanup.
- Cleaned independently certified save sections without letting an unrelated incomplete section stall all cleanup.
- Verified exact pre-prune backups before character or supported world-save mutation.
- Used native client quest-removal notification ordering before replacing authority tracking.
- Applied relaxed native loading first, with acceptance bypass as the verified fallback after native rejection.
- Added character-scoped game notifications for attributed cleanup, skipped sections and fallback events.
- Kept uncertain identities, failed backups and unsupported save layouts protected from guessed cleanup.
- Documented cooked-only repair item and exact-output bench recipe boundaries without adding a custom repair runtime action.

### Diagnostics, reporting and documentation

- Replaced the Live Log navigation destination with a consolidated Diagnostics workspace.
- Combined overview, grouped issues and Live Log without the redundant Live Log/Advanced Diagnostics outer tabs.
- Added structured JSONL records, retained sessions, issue mutes and reviewed local report export.
- Retained original log messages, severity, source, outcome and unknown-cause distinctions.
- Read ordinary RuneSchema.log activity even when the structured error file is empty.
- Added Game and EOS log views alongside RuneSchema and UE4SS.
- Added phrase/exclusion/source/code/level filtering, pause/follow and bounded grouped-repeat views.
- Added 221 structured diagnostic definitions alongside the expanded legacy reference catalogue.
- Added 16 decision-site codes for configuration, plugin startup and world-cleanup safeguards.
- Published 283 distinct RS code labels and 19 bridge reasons with area/family sections and preserved direct links.
- Preserved separate legacy/structured meanings for the three shared code labels.
- Used explicit registered areas before keyword hints when grouping diagnostics.
- Replaced numeric catalogue-position hints with named IDs so new codes cannot shift old references.
- Added generation, emission, family, namespace and rendered-heading checks for the shared diagnostic reference.
- Added a categorized Report a bug form with preview/copy and prefilled GitHub issue submission.
- Kept final submission in the user's browser and omitted embedded bot tokens or automatic log uploads.
- Added numbered authoring walkthroughs, native-registration boundaries, server guidance and downloadable structural examples.

## Removed or retired

These include experiments introduced and retired during 0.8.0.1 development;
they were not necessarily features of the published 0.8.0.0 baseline.

- Removed the experimental Scrap action and automatic refund behavior.
- Removed the live Delete observer and deletion interception used by Scrap/storage experiments.
- Removed the deleted-item Helpy Storage tab and automatic deleted-item capture.
- Removed the deleted-item restore action and retired Scrap UI settings; legacy keys remain harmless.
- Retired companion Recovery restoration in favor of verified backups before pruning.
- Removed the Recovery Quick settings toggle; retained old archives without automatic restore or deletion.
- Abandoned `.runesave`/replacement-save conversion as an active runtime feature; native saves remain authoritative.
- Removed startup root-layout migration and the requirement to close other runtimes merely to migrate AppData.
- Removed the old install-local live-runtime storage dependency and shipped mapping directory.
- Retired the optional in-game Canvas/quick-menu experiment; the separate F2 Helpy desktop remains.
- Retired SchemaVision and its overlay/preview experiment rather than adding another renderer to the game.
- Removed duplicate Diagnostics outer tabs, the obsolete Live Log navigation label and the duplicate Home header runtime badge.
- Removed plain visible RSMK title text from the main custom caption while keeping an accessible native title.
- Removed unrelated per-call bridge path/string work and recurring DPI polling where cached/event-driven paths suffice.
- Removed the repair-specific runtime prototype in favor of ordinary registration plus cooked item behavior.
- Excluded optional UI artwork, cached mappings, personal data, build outputs and UE4SS.dll from the cleaned full ZIP.

## Verification and boundaries

- Passed 240 native offline tests and 102 Helpy tests for the matching DLL build.
- Passed seven diagnostic-generation/audit tests and JavaScript search/direct-link checks.
- Passed strict website builds and rendered code/family-anchor checks.
- Passed UPX integrity and decompression comparisons of code, data, imports/IAT slots and resources.
- Passed compressed Helpy hidden-start and dedicated-desktop plugin contracts.
- Matched both DLL hashes inside the supplied cleaned release ZIP to those verified outputs.
- Did not treat automated tests as a live gameplay/frame-time measurement or proof of every cooked-family consumer.
- Kept current unsupported native/hybrid nested UE4SS packages outside the Lua-only companion loader.
- Kept cooked-only repair gameplay, native skill-enum extension and a standalone Helpy EXE out of the shipped feature claims.

The archive is a local, user-supplied package refresh, not a new hosted CI build.
Read [the release notes](RELEASE-0.8.0.1.md) for exact hashes, defaults and scope.
