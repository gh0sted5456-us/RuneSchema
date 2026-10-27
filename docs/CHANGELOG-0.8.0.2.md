# RuneSchema and Helpy 0.8.0.2 — changes since 0.8.0.1

This changelog includes the 0.8.0.1b fixes, the October 25, 2026 release and the
October 27 source maintenance update. Both DLLs continue to identify as 0.8.0.2.

**Source and download status:** the October 27 maintenance changes are published
to `main` and `experimental`. The tagged October 25 release archive remains the
published download; publishing branch source does not replace its DLLs or tag.
See [downloads and source updates](CURRENT-RELEASE.md) and the
[October 25 package record](RELEASE-0.8.0.2.md).

## Registries and save cleanup

- Added cooked building enrollment and build-menu placement.
- Added JSONC fallback examples for older cooked mods that cannot be recooked.
- Made registry-path deduplication, policy conflicts and Schema provenance checks case-insensitive.
- Preserved existing declarations when a registration batch fails.
- Added `RS-BLD-023` guidance for rejected cooked building registrations.
- Allowed readable inventory and storage sections to continue cleanup when another section has an unfamiliar format.
- Preserved unfamiliar quest tracking and location dependencies while pruning verified orphan quests.
- Prevented invalid equipment indices from wrapping into unrelated inventory slots.
- Removed repeated whole-save copies during section-by-section cleanup.

## Caches and Item Bay

- Separated the GitHub cache catalogue from DLL release tags so cache updates can ship independently.
- Added more tolerant base-game cache selection across game versions while retaining live gameplay validation.
- Added owner-specific fingerprints for mods sharing the same mounted namespace.
- Made cache identity resilient to installation-folder changes through content fingerprints and owner aliases.
- Moved item-icon caches into each platform's AppData cache directory.
- Added cache-first and game-first loading choices.
- Enabled passive content updates by default.
- Fixed downloaded vanilla caches preventing discovery of uncached modded items.
- Made searches fall through to bounded live discovery when cached results are unavailable.
- Added enabled-container package discovery for mod items omitted from the native item registry.
- Improved PAK discovery behind Game Pass and mod-manager directory redirects.
- Saved newly discovered item references asynchronously for reuse in later searches and launches.
- Merged downloaded cache content with locally learned items rather than replacing those discoveries.
- Excluded disabled or removed mod providers from cached search results.
- Reused saved catalogue pages and sorted indexes instead of rebuilding the entire catalogue for each search.
- Preserved visible results during incomplete background refreshes.
- Preserved unrelated icons and capture requests when another source changes.
- Added long-path support for cache download staging, verification and publication.
- Retained completed downloads while their imports wait for source verification or another cache operation.
- Published a partial Steam hotfix cache containing 2,903 item/journal records and 4,246 pictures.
- Made the About service use the same version as the plugin descriptor.

See [cache bundles](CACHE-BUNDLES.md) and [icon caching](HELPY-ICON-CACHE.md).
Cached references remain presentation data; delivery and other gameplay actions
still validate the running game's content and authority.

## Cart and item selection

- Added right-click **Reorder** for confirmed Last delivery items and the delivery heading.
- Added **Reorder selected** for checked delivery entries.
- Added cart selection through checkboxes and Ctrl+click.
- Added **Select all**, including an indicator for partial selection.
- Added **Up**, **Down** and **Remove selected** controls.
- Added **Give selected** and clarified **Give all items**.
- Moved each Select checkbox beside its quantity.
- Adjusted narrow layouts so quantity controls and action buttons stay within their cards.
- Disabled quantity step buttons at their limits.
- Made **Import**, **Export** and **Clear** directly accessible.
- Added loading multiple presets, optional merging with the current cart, and export of selected items.
- Kept reordered deliveries staged for review rather than sending them automatically.

See [cart selection, delivery and presets](HELPY-CART-RECIPIENTS.md).

## Map and teleport

- Reused completed ground samples across teleport preparation updates.
- Limited ground-probe work per update to keep preparation and cancellation responsive.
- Added teleport phase, elapsed-time and completion reporting.
- Added cached map artwork and coordinate alignment to compatible base-game cache bundles.
- Allowed dedicated-server Helpy to display imported map artwork without a client graphics capture.
- Kept cached map presentation separate from live ground and placement validation.
- Added **Export current cache** without requiring Build first or rejecting an existing snapshot solely because installed content changed.
- Reorganized cache operations around importing content, optionally filling gaps, and exporting the current cache.

See [map travel](HELPY-MAP-TRAVEL.md). These changes do not establish that every
reported graphics crash is resolved.

## Authoring tools and documentation

- Added optional offline cache builders for native content and mod PAKs.
- Added an authoring-tools ZIP download through Helpy.
- Added **Build-Helpy-Cache.bat** for dragging and dropping a mod ZIP.
- Supported nested ZIP folders containing a cooked PAK trio and optional `background.png`.
- Added automatic discovery of inactive game installations, mappings and decoder inputs.
- Added optional `.usmap` input and verified RSDW Archive mapping fallback.
- Fixed progress-calculation overflow with large mod ZIPs.
- Added progress reporting for extraction, hashing, discovery, picture capture and publication.
- Preserved previous cache output when a build fails.
- Added locked dependency restoration, source/package parity checks and author-tool CI.
- Added worked guides for FModel exports, creature drops, cooked-item overrides, `$Clone`, `$Append` and targeted patching.
- Simplified the GitHub overview and directed detailed installation, authoring and diagnostic guidance to the documentation website.

See the [offline cache-builder walkthrough](modders/offline-helpy-cache.md).

## Networking and servers

- Expanded the general data relay from 64 to 256 declared channels.
- Added optional complete messages up to 1 MiB; the default remains 64 KiB.
- Retained bounded chunks, paced delivery, checksums and memory limits.
- Added an experimental server-authoritative registry-ID alignment handshake before world loading.
- Preserved protected native registry prefixes and compatible client-only entries.
- Compressed the authority roster and ID ordering to fit the existing 48 KiB welcome limit.
- Made compatible mod-version and ordinary content differences warning-only through `RS-NET-002`.
- Included generated quest/journal registrations in synchronization ownership tracking.
- Added guarded native dedicated-server name-filter initialization to address filter-worker stalls.
- Added `RS-SRV-001` and `RS-SRV-002` diagnostics for that startup path.
- Prevented pending Helpy asynchronous tasks from blocking DLL process teardown.

Registry-ID alignment remains **experimental and disabled by default**. Isolated
authenticated Game Pass tests verified world entry, reconnect, one native item
replication and one recipe-ID notification. Complete crafting and simultaneous
authenticated-client gameplay remain unverified. An ID list cannot supply
missing cooked assets. See [transport limits](PRESENTATION-TRANSPORT.md).

## Quick settings and diagnostics

- Added a Helpy shortcut selector with **F2 as the default**.
- Added **Reset to F2**.
- Saved the shortcut through `[RuneSchema] helpyHotkey` in `settings.ini`.
- Applied shortcut changes immediately when saved through Helpy.
- Updated shortcut hints and startup messages to display the selected key.
- Prevented held keys from immediately toggling Helpy after rebinding.
- Preserved text entry when using letter or navigation-key shortcuts.
- Removed the dedicated F5 theme-reload binding and its polling.
- Added **Reload theme** to Quick settings.
- Added an optional full-memory crash-dump setting with configuration backups.
- Updated diagnostic references and authoring documentation.

See [shortcut and diagnostic settings](SETTINGS-INI.md).

## Build verification and scope

- Passed 248 core tests and 109 Helpy tests for the latest maintenance build.
- Checked rendered Quick settings at 1280×900, 960×700 and 760×560.
- Passed strict documentation builds and compressed-plugin boundary checks.
- Verified both compressed DLLs through UPX integrity and decompression comparisons.
- Installed and independently hash/version-verified both DLLs on Steam, Game Pass and the dedicated server, with backups.

The final hotkey build has not received an in-game gameplay test. Automated
checks are not frame-time measurements or a compatibility guarantee for every
mod. The retired Scrap workflow, companion Recovery restoration, replacement
save format and standalone Helpy EXE remain absent. Skill/SkillPerk declarations
do not become new native gameplay registries in this update.
