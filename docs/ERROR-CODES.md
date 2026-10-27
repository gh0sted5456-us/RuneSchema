# Diagnostic code reference

Find a code from your log to see what happened and what to try next. Use the message and severity shown in your own log to judge the event; a code by itself does not mean the game crashed. **Suggested references** help with older uncoded messages, without assigning them a confirmed cause.

Open Helpy's **Diagnostics** for an overview, grouped issues and Live Log. You can copy an entry or follow its reference link here. The viewer leaves the original logs unchanged; recent activity includes normal messages, while notifications focus on warnings and errors.

This page contains **71 legacy reference entries**, **222 structured diagnostic entries** and **19 bridge reason codes**. RS-JRN-001, RS-JRN-002 and RS-SPAWN-001 each have an older and a structured meaning. Follow the link from your message, or check its source and namespace to choose the matching entry. Links between the two meanings are included.

Structured reports in `RSDiagLog.jsonl` include their source, context and outcome. Not every operation emits every code. `Unknown` means the cause has not been identified; use the severity in the actual message. `Partial` means some changes may have occurred, and `Deferred` points to later or restart handling. `Skipped` or `Blocked` can describe one step while the rest of the mod continues.

## Find your next step {#read-a-code-safely}

1. Open the reference link in Helpy's Diagnostics or Live Log to jump straight to your code. Shared legacy/structured labels include links to both meanings.
2. Read your original message and severity, then look for the first related failure nearby. The typical level on this page is a guide; your event's reported level takes precedence.
3. Try the checks listed for that condition, keeping the last working cache and original saves. If you still need help, share the code, subsystem, game/platform/RuneSchema versions and a short log excerpt with private details removed.

!!! note "Sharing a log excerpt"
    Check for player names, character IDs and local paths before posting. Opening this reference page does not send your log content.

<div class="rs-code-tools"><label for="rs-code-search">Find a code or symptom</label><input id="rs-code-search" type="search" placeholder="RS-CACHE-003, bridge_not_ready, chest, journal..." autocomplete="off"><p id="rs-code-count" aria-live="polite"></p></div>

## Code families

| Family | Area |
| --- | --- |
| [RS-ASSET](#structured-family-rs-asset) | Assets |
| [RS-BLD](#structured-family-rs-bld) | Buildings |
| [RS-BP](#structured-family-rs-bp) | Blueprints |
| [RS-CACHE](#legacy-family-rs-cache) | Cache |
| [RS-CAT](#legacy-family-rs-cat) | Items |
| [RS-CFG](#legacy-family-rs-cfg) | Configuration |
| [RS-CHEST](#legacy-family-rs-chest) | Storage |
| [RS-CORE](#structured-family-rs-core) | Runtime |
| [RS-CRS](#structured-family-rs-crs) | Courses |
| [RS-DL](#legacy-family-rs-dl) | Downloads |
| [RS-ENUM](#structured-family-rs-enum) | Enums |
| [RS-EQP](#structured-family-rs-eqp) | Equipment |
| [RS-EQUIP](#legacy-family-rs-equip) | Equipment |
| [RS-GAME](#legacy-family-rs-game) | Game items |
| [RS-GE](#structured-family-rs-ge) | Gameplay effects |
| [RS-IMP](#structured-family-rs-imp) | Imports |
| [RS-ITEM](#legacy-family-rs-item) | Items |
| [RS-JRN](#legacy-family-rs-jrn) | Journal |
| [RS-JSON](#structured-family-rs-json) | Configuration and definitions |
| [RS-LOAD](#structured-family-rs-load) | Loaders |
| [RS-LOG](#legacy-family-rs-log) | Console |
| [RS-MAP](#legacy-family-rs-map) | Map |
| [RS-NET](#legacy-family-rs-net) | Multiplayer |
| [RS-NIA](#structured-family-rs-nia) | Niagara |
| [RS-NPC](#structured-family-rs-npc) | NPCs |
| [RS-PARTY](#legacy-family-rs-party) | Party |
| [RS-PERF](#legacy-family-rs-perf) | Performance |
| [RS-PLUG](#legacy-family-rs-plug) | Plugins |
| [RS-QST](#legacy-family-rs-qst) | Quests |
| [RS-RAW](#structured-family-rs-raw) | Data tables |
| [RS-REC](#structured-family-rs-rec) | Recipes |
| [RS-REG](#legacy-family-rs-reg) | Registry |
| [RS-RGST](#structured-family-rs-rgst) | Registry |
| [RS-RT](#legacy-family-rs-rt) | Runtime |
| [RS-SAVE](#legacy-family-rs-save) | Save safety |
| [RS-SPAWN](#legacy-family-rs-spawn) | Spawns |
| [RS-SRC](#legacy-family-rs-src) | Content |
| [RS-SRV](#legacy-family-rs-srv) | Server |
| [RS-STR](#structured-family-rs-str) | Strings |
| [RS-VND](#structured-family-rs-vnd) | Vendors |
| [RS-WEAR](#legacy-family-rs-wear) | Equipment |
| [RS-WORLD](#legacy-family-rs-world) | World save cleanup |

<div class="rs-code-catalogue" markdown="1">

## Cache — RS-CACHE {#legacy-family-rs-cache}

<section class="rs-code-card" data-rs-code="RS-CACHE-001" data-rs-severity="info" markdown="1">

### RS-CACHE-001: Mounted content changed {#rs-cache-001}

**Cache · Typical level: info**

Content identity changed during cache work.

**Common reasons**

A mod, mounted source list or source generation changed while cache work was in progress.

**What RuneSchema does**

The stale result is not committed under the new identity.

**What to do next**

Keep last-good results while the affected owner is rechecked.

**Troubleshooting checks**

Let the source list settle, retain the last-good cache and retry the affected owner's work. Avoid starting repeated full sweeps while mods are still mounting.

[Direct link to RS-CACHE-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-001)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-002" data-rs-severity="error" markdown="1">

### RS-CACHE-002: Cache bundle format is invalid {#rs-cache-002}

**Cache · Typical level: error**

The header, version, manifest or record bounds failed validation.

**Common reasons**

The file is truncated, uses an unsupported bundle version, or contains invalid manifest fields, record identities, bounds or artwork metadata.

**What RuneSchema does**

The invalid bundle is not activated.

**What to do next**

Download the original bundle again or ask its publisher for a supported export.

**Troubleshooting checks**

Check that the selected file is an original .rsmk-cache or supported legacy .rsmk-icons export. Download it again or ask the publisher for a current export. Renaming another archive does not convert it.

[Direct link to RS-CACHE-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-002)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-003" data-rs-severity="warning" markdown="1">

### RS-CACHE-003: Cache does not match installed content {#rs-cache-003}

**Cache · Typical level: warning**

The portable identity differs or a required source is inactive.

**Common reasons**

The installed owner fingerprints, game build, required sources or locale do not match the bundle group.

**What RuneSchema does**

Only matching source groups are used; unrelated owners remain intact.

**What to do next**

Use the matching build/mod or local Build / update for the affected source.

**Troubleshooting checks**

Inspect the affected owner and locale. Download a matching pack or use local Build / update for that owner. Unrelated matching groups can remain in use; a cache does not install the missing mod.

[Direct link to RS-CACHE-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-003)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-004" data-rs-severity="error" markdown="1">

### RS-CACHE-004: Cache integrity check failed {#rs-cache-004}

**Cache · Typical level: error**

Bytes do not match their declared SHA-256 or pixel layout.

**Common reasons**

Downloaded or cached bytes differ from the declared SHA-256, or the pixel extent is invalid.

**What RuneSchema does**

Unverified bytes are not accepted as valid cache data.

**What to do next**

Retry the original download; never bypass checksums or rename an unrelated pack.

**Troubleshooting checks**

Retry the original publication and check available disk space. If the same publication still fails, report the pack identity to its owner. Never disable checksum verification.

[Direct link to RS-CACHE-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-004)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-005" data-rs-severity="warning" markdown="1">

### RS-CACHE-005: Cache storage limit reached {#rs-cache-005}

**Cache · Typical level: warning**

The operation exceeds the configured size or record limit.

**Common reasons**

The import/export exceeds configured disk usage, metadata, record or artwork limits.

**What RuneSchema does**

The operation is refused without removing unrelated owners.

**What to do next**

Increase the limit or use explicit old-cache cleanup.

**Troubleshooting checks**

Read which limit was exceeded. Increase an exposed cache budget if appropriate, or use explicit old-cache cleanup and a smaller owner-specific bundle. Do not remove mod sources or saves.

[Direct link to RS-CACHE-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-005)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-006" data-rs-severity="info" markdown="1">

### RS-CACHE-006: Cache operation cancelled {#rs-cache-006}

**Cache · Typical level: info**

A user request or changed source generation cancelled work.

**Common reasons**

The user cancelled work, or a source-generation change invalidated the in-flight operation.

**What RuneSchema does**

Valid cached content remains usable; incomplete metadata is not activated.

**What to do next**

Retry when convenient; cancellation alone is not corruption.

**Troubleshooting checks**

No repair is needed for cancellation alone. Keep committed cache content and retry after mounted sources stabilize if the operation is still wanted.

[Direct link to RS-CACHE-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-006)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-007" data-rs-severity="warning" markdown="1">

### RS-CACHE-007: Bundle has partial coverage {#rs-cache-007}

**Cache · Typical level: warning**

Only the declared observed subset was captured.

**Common reasons**

A bundle captures an observed subset, or export found no indexed presentation records/artwork yet.

**What RuneSchema does**

Missing categories and art can still use local discovery.

**What to do next**

Check coverage by category; a full sweep is optional.

**Troubleshooting checks**

Read the coverage report and original event. Observe or build the missing category locally, or import a compatible fuller bundle. Do not interpret missing cached entries as removed gameplay content.

[Direct link to RS-CACHE-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-007)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-008" data-rs-severity="warning" markdown="1">

### RS-CACHE-008: Content owner cannot be verified {#rs-cache-008}

**Cache · Typical level: warning**

A record claims ownership unsupported by active source evidence.

**Common reasons**

A record's claimed owner lacks matching active-source or exact asset evidence.

**What RuneSchema does**

The record is skipped; borrowed artwork never reassigns content ownership.

**What to do next**

Correct the publisher manifest or exact asset declarations.

**Troubleshooting checks**

Inspect the owner manifest and declared asset paths. Ask the publisher to correct and re-export the bundle. Shared/borrowed artwork must not change the item's owning mod.

[Direct link to RS-CACHE-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-008)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-009" data-rs-severity="error" markdown="1">

### RS-CACHE-009: Cache update could not be committed {#rs-cache-009}

**Cache · Typical level: error**

A staged cache update could not be safely activated.

**Common reasons**

A staged cache file could not be written, flushed, renamed or activated safely.

**What RuneSchema does**

The previous committed metadata remains usable.

**What to do next**

Check permissions and free space, then retry; keep the last-good cache.

**Troubleshooting checks**

Check the destination's permissions and free space and preserve the last committed metadata. Retry once those are resolved; do not manually activate a partially written staging file.

[Direct link to RS-CACHE-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-009)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-010" data-rs-severity="info" markdown="1">

### RS-CACHE-010: Legacy icon pack loaded {#rs-cache-010}

**Cache · Typical level: info**

This is an older pixel-only .rsmk-icons pack.

**Common reasons**

An older pixel-only icon pack was imported instead of a metadata-bearing cache bundle.

**What RuneSchema does**

Verified pictures can be reused, but it supplies no item or journal records.

**What to do next**

Use a newer .rsmk-cache bundle when published; a sweep is optional.

**Troubleshooting checks**

Pictures may still be useful; this is not an integrity failure. Import a compatible .rsmk-cache for item identities, pathing, persistence IDs and journal records when available.

[Direct link to RS-CACHE-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-010)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-011" data-rs-severity="info" markdown="1">

### RS-CACHE-011: Helpy content loading preference {#rs-cache-011}

**Cache · Typical level: info**

An item query selected cache-first or game-first browsing and its passive refresh policy.

**Common reasons**

An item query selected cache-first or game-first browsing and its passive refresh policy.

**What RuneSchema does**

This controls item presentation and bounded discovery; live action validation remains enabled.

**What to do next**

Use Quick settings > Content & cache to choose a loading order and passive updates.

**Troubleshooting checks**

Use Quick settings > Content & cache to choose a loading order and passive updates. This controls item presentation and bounded discovery; live action validation remains enabled.

[Direct link to RS-CACHE-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-011)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-012" data-rs-severity="info" markdown="1">

### RS-CACHE-012: Cached references used as fallback {#rs-cache-012}

**Cache · Typical level: info**

Game-first browsing has no ready or matching live item page and is displaying saved references.

**Common reasons**

Game-first browsing has no ready or matching live item page and is displaying saved references.

**What RuneSchema does**

Pictures and references remain available; a reference alone does not prove an item is loaded or deliverable.

**What to do next**

Wait for bounded discovery, search again, or use Cache first for immediate browsing.

**Troubleshooting checks**

Wait for bounded discovery, search again, or use Cache first for immediate browsing. Pictures and references remain available; a reference alone does not prove an item is loaded or deliverable.

[Direct link to RS-CACHE-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-012)

</section>

<section class="rs-code-card" data-rs-code="RS-CACHE-013" data-rs-severity="error" markdown="1">

### RS-CACHE-013: Unsupported content loading preference {#rs-cache-013}

**Cache · Typical level: error**

The cache loadOrder setting is neither cacheFirst nor gameFirst.

**Common reasons**

The cache loadOrder setting is neither cacheFirst nor gameFirst.

**What RuneSchema does**

The invalid preference is rejected; the original configuration is retained.

**What to do next**

Correct cache.loadOrder in ui.jsonc, then reload Helpy. Use cacheFirst or gameFirst.

**Troubleshooting checks**

Correct cache.loadOrder in ui.jsonc, then reload Helpy. Use cacheFirst or gameFirst. The invalid preference is rejected; the original configuration is retained.

[Direct link to RS-CACHE-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cache-013)

</section>

## Items — RS-CAT {#legacy-family-rs-cat}

<section class="rs-code-card" data-rs-code="RS-CAT-001" data-rs-severity="warning" markdown="1">

### RS-CAT-001: Item catalogue is not ready {#rs-cat-001}

**Items · Typical level: warning**

A catalogue or cursor is unavailable, changed or expired.

**Common reasons**

The item catalogue is not ready, or a query cursor belongs to an older content generation.

**What RuneSchema does**

Last-good cached presentation remains usable while the query is rechecked.

**What to do next**

Refresh once; inspect mounted-content changes if it persists.

**Troubleshooting checks**

Keep the last-good results visible. Refresh the affected query once after source changes settle. If it repeats, include the search text, category/source filters and first catalogue error in a report.

[Direct link to RS-CAT-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cat-001)

</section>

## Configuration — RS-CFG {#legacy-family-rs-cfg}

<section class="rs-code-card" data-rs-code="RS-CFG-001" data-rs-severity="error" markdown="1">

### RS-CFG-001: Configuration could not be loaded {#rs-cfg-001}

**Configuration · Typical level: error**

Configuration loading failed; session defaults are being used.

**Common reasons**

Reading, validating or initializing the runtime configuration failed. The original exception distinguishes syntax, type and access failures.

**What RuneSchema does**

Existing configuration files are retained. The session may not use the requested settings.

**What to do next**

Read the original exception and named configuration path. Correct the file or access problem before restarting.

**Troubleshooting checks**

Inspect the named settings file and original exception. The runtime is using session defaults; preserve the original instead of deleting unrelated caches or saves.

[Direct link to RS-CFG-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cfg-001)

</section>

<section class="rs-code-card" data-rs-code="RS-CFG-002" data-rs-severity="warning" markdown="1">

### RS-CFG-002: Configuration saved with original backup {#rs-cfg-002}

**Configuration · Typical level: warning**

An explicit save replaced preserved configuration with current settings after backing up the original.

**Common reasons**

Current settings were explicitly saved after configuration recovery, and an original-file backup was created.

**What RuneSchema does**

The original file is retained at the reported backup path; startup settings require restart.

**What to do next**

Review the saved settings and retain the backup if an original value needs to be recovered.

**Troubleshooting checks**

Review the reported backup path and resulting settings. Restart to apply startup-only preferences; restore individual original values only after checking their validity.

[Direct link to RS-CFG-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cfg-002)

</section>

<section class="rs-code-card" data-rs-code="RS-CFG-003" data-rs-severity="error" markdown="1">

### RS-CFG-003: Configuration save failed {#rs-cfg-003}

**Configuration · Typical level: error**

The settings save was not completed.

**Common reasons**

Settings serialization, validation, backup or writing reported a failure.

**What RuneSchema does**

Changes remain session-only. Any backup path in the original message should be retained.

**What to do next**

Check the exact write/validation error and path; do not assume preferences persisted.

**Troubleshooting checks**

Read the exact exception and any backup path. Check permissions and free space, then reload before retrying; session values are not proof of a successful disk save.

[Direct link to RS-CFG-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-cfg-003)

</section>

## Storage — RS-CHEST {#legacy-family-rs-chest}

<section class="rs-code-card" data-rs-code="RS-CHEST-001" data-rs-severity="warning" markdown="1">

### RS-CHEST-001: Chest access contract is unverified {#rs-chest-001}

**Storage · Typical level: warning**

Lock ownership, deposit permissions or persistence semantics are unverified.

**Common reasons**

The native lock, owner, deposit permission or persistence contract is not verified for this storage type.

**What RuneSchema does**

Direct deposits and unlock overrides stay disabled.

**What to do next**

Use the game interaction until its current native access path is verified.

**Troubleshooting checks**

Use normal game interaction. Keep direct deposits and unlock overrides disabled until this build's access contract is supported; do not infer permission from map visibility.

[Direct link to RS-CHEST-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-chest-001)

</section>

<section class="rs-code-card" data-rs-code="RS-CHEST-002" data-rs-severity="warning" markdown="1">

### RS-CHEST-002: Storage inventory is ambiguous {#rs-chest-002}

**Storage · Typical level: warning**

The building has no unique owned native inventory.

**Common reasons**

The selected building has no unique owned native inventory, or multiple components could be mistaken for it.

**What RuneSchema does**

Contents are unavailable, not read from an unrelated component.

**What to do next**

Refresh the target and inspect its native component contract.

**Troubleshooting checks**

Refresh and inspect the selected building's component contract. Keep contents unavailable rather than selecting an unrelated inventory. Do not deposit until the inventory is uniquely identified.

[Direct link to RS-CHEST-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-chest-002)

</section>

## Downloads — RS-DL {#legacy-family-rs-dl}

<section class="rs-code-card" data-rs-code="RS-DL-001" data-rs-severity="warning" markdown="1">

### RS-DL-001: Download is unavailable {#rs-dl-001}

**Downloads · Typical level: warning**

A download failed or no compatible publication exists.

**Common reasons**

Connectivity failed, or the publication index has no pack compatible with the active build and sources.

**What RuneSchema does**

Cached content and local Build / update remain available.

**What to do next**

Check connectivity and the supported build list, then retry later.

**Troubleshooting checks**

Check the original download result and supported-build list. Retry later if offline; otherwise use local Build / update. Existing caches remain available.

[Direct link to RS-DL-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-dl-001)

</section>

<section class="rs-code-card" data-rs-code="RS-DL-002" data-rs-severity="error" markdown="1">

### RS-DL-002: Download manifest was rejected {#rs-dl-002}

**Downloads · Typical level: error**

Remote filename, index, size or integrity metadata is unsupported.

**Common reasons**

The remote index version, filename, nesting, size or integrity metadata is unsupported or unsafe.

**What RuneSchema does**

The download is not imported.

**What to do next**

Use a supported publisher index and keep the previous cache.

**Troubleshooting checks**

Use the project's supported publication index. Report the rejected index/pack identity to its publisher. Keep the previous cache and do not bypass path or integrity checks.

[Direct link to RS-DL-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-dl-002)

</section>

## Equipment — RS-EQUIP {#legacy-family-rs-equip}

<section class="rs-code-card" data-rs-code="RS-EQUIP-001" data-rs-severity="warning" markdown="1">

### RS-EQUIP-001: Auto-equip is unsupported or ambiguous {#rs-equip-001}

**Equipment · Typical level: warning**

An instance, slot or native contract cannot be confirmed.

**Common reasons**

The exact delivered item instance, supported equipment slot or native equip contract is ambiguous.

**What RuneSchema does**

Delivery and equip status stay separate; no duplicate grant is made.

**What to do next**

Equip manually or inspect the named instance and slot.

**Troubleshooting checks**

Check delivery results separately, then equip the existing item manually if needed. Do not deliver it again merely because automatic equipping was unavailable.

[Direct link to RS-EQUIP-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-equip-001)

</section>

<section class="rs-code-card" data-rs-code="RS-EQUIP-002" data-rs-severity="warning" markdown="1">

### RS-EQUIP-002: Equip result is not confirmed {#rs-equip-002}

**Equipment · Typical level: warning**

Exact-instance readback does not prove the expected result.

**Common reasons**

The native call returned but exact-instance equipment readback did not confirm the intended slot/item.

**What RuneSchema does**

A native acceptance flag alone is not reported as successful equipping.

**What to do next**

Check inventory and loadout before retrying.

**Troubleshooting checks**

Inspect inventory and loadout before retrying. Record the item and slot. A native acceptance flag is not proof of equipping and should not trigger another grant.

[Direct link to RS-EQUIP-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-equip-002)

</section>

## Game items — RS-GAME {#legacy-family-rs-game}

<section class="rs-code-card" data-rs-code="RS-GAME-000" data-rs-severity="warning" markdown="1">

### RS-GAME-000: Native game warning or error {#rs-game-000}

**Game · Typical level: warning**

A native game warning or error has no more specific RuneSchema reference yet.

**Common reasons**

No specific classifier matched this native category and message. The original error may include an engine, EOS, HTTP or platform-owned code.

**What RuneSchema does**

The original message and severity remain authoritative. This reference is not a diagnosis or a game-issued error code.

**What to do next**

Keep the original category, message, provider code and surrounding events. Report the first failure and when it occurred.

**Troubleshooting checks**

Read the original message and first preceding failure. Keep provider codes unchanged. Record game build, storefront and whether the event occurred during startup, play or shutdown; this generic reference does not claim a root cause.

[Direct link to RS-GAME-000](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-000)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-001" data-rs-severity="error" markdown="1">

### RS-GAME-001: Item power-level row name is invalid {#rs-game-001}

**Game items · Typical level: error**

The game cannot interpret the power-level digits in an item's data-table row name.

**Common reasons**

ChangePowerLevelInRowName rejected a row whose expected numeric positions contain non-digits. This often indicates authored or cloned row naming, not an obsolete USMAP.

**What RuneSchema does**

The native power-level update for that item failed; a mapping refresh does not rename cooked rows.

**What to do next**

Report the named item and its row name to the content author. Correct the native row naming scheme and recook the asset.

**Troubleshooting checks**

Keep the exact item and row name. Compare it with a working native item of the same type and power-level family. Repair the authoring definition and recook; do not delete unrelated save content or suppress the error.

[Direct link to RS-GAME-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-001)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-002" data-rs-severity="warning" markdown="1">

### RS-GAME-002: Cooked animation skeleton GUID is stale {#rs-game-002}

**Game animation · Typical level: warning**

A cooked animation refers to an older skeleton identity.

**Common reasons**

The engine found an animation/skeleton identity mismatch left in cooked content. Registry enrollment does not repair that cooked relationship.

**What RuneSchema does**

The engine reports an asset consistency warning; this alone is not proof of a corrupt save.

**What to do next**

The author should resave the affected source animation against its current skeleton and recook it.

**Troubleshooting checks**

Check the exact montage or sequence in the message. Resave it in the matching Unreal project against the intended skeleton, recook and verify that the warning disappears on the target platform.

[Direct link to RS-GAME-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-002)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-003" data-rs-severity="warning" markdown="1">

### RS-GAME-003: Native package or object was not found {#rs-game-003}

**Game packages · Typical level: warning**

An engine package or object lookup did not resolve at the reported path.

**Common reasons**

A package may be unmounted, incorrectly named or unavailable; alternatively an optional conventional registry lookup may legitimately be absent while another declaration works.

**What RuneSchema does**

A required asset may be missing, or an optional conventional-path probe may have found no asset. Read the surrounding declaration result.

**What to do next**

Check the exact cooked package path and mount. Do not mark a mod broken solely because an optional guessed registry path is absent.

**Troubleshooting checks**

Read the owning registrar's accepted/rejected result and exact path. Verify archive contents and mounts before changing declarations. Do not invent a native asset or treat cache artwork as package registration.

[Direct link to RS-GAME-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-003)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-004" data-rs-severity="error" markdown="1">

### RS-GAME-004: Material shader data is invalid {#rs-game-004}

**Game materials · Typical level: error**

The engine could not use a material's cooked shader map.

**Common reasons**

The named material lacks a usable shader map for this game/platform/cook, or the engine reports an unnamed invalid material resource.

**What RuneSchema does**

The affected material may render incorrectly; the warning is not a persistence-ID diagnosis.

**What to do next**

Check the named material, target platform and cooking settings. Use a matching cooked build; preserve the original error.

**Troubleshooting checks**

Keep surrounding asset-loading errors and game/storefront versions. Compare the package with the correct cooked target. A USMAP rebuild cannot manufacture missing shader data.

[Direct link to RS-GAME-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-004)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-005" data-rs-severity="error" markdown="1">

### RS-GAME-005: Blueprint accessed a missing object {#rs-game-005}

**Game scripts · Typical level: error**

A native Blueprint message reports an Accessed None object reference.

**Common reasons**

A Blueprint attempted to access an object that is absent, destroyed or not initialized at the point of use.

**What RuneSchema does**

The reported Blueprint operation may not have completed. This reference does not identify who caused the missing object.

**What to do next**

Capture the Blueprint/function and first preceding error. The content author should validate that object before using it.

**Troubleshooting checks**

Retain the native Blueprint, function and call-site text. Inspect initialization order and earlier load failures. A suggested reference is guidance, not proof of mod ownership or a completed action.

[Direct link to RS-GAME-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-005)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-006" data-rs-severity="warning" markdown="1">

### RS-GAME-006: Vitals UI callback cleanup failed {#rs-game-006}

**Game UI · Typical level: warning**

The native vitals widget could not remove a registered callback.

**Common reasons**

A native vitals-widget callback could not be removed. This can occur while widgets or their owners are being destroyed.

**What RuneSchema does**

The widget may be tearing down. This message alone does not establish save corruption or mod failure.

**What to do next**

Check whether this occurs only during exit or also during play. Preserve the callback name and earlier UI errors.

**Troubleshooting checks**

Record the exact callback and whether the UI malfunctions while playing. Compare the timestamp with world exit and shutdown. Do not mark a mod unhealthy solely because of an unattributed teardown warning.

[Direct link to RS-GAME-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-006)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-007" data-rs-severity="warning" markdown="1">

### RS-GAME-007: Native user settings unavailable {#rs-game-007}

**Game settings · Typical level: warning**

The game could not find its expected native user settings object.

**Common reasons**

A native settings access occurred before initialization, after teardown, or while its expected settings object was unavailable.

**What RuneSchema does**

Settings access may fail. During shutdown the object may already be unavailable; this is not evidence of a corrupt character save.

**What to do next**

Check the timing and whether settings still work while playing. Capture preceding initialization errors; do not delete saves to silence this warning.

**Troubleshooting checks**

Check whether settings work during normal play. Preserve the earliest settings initialization failure. A shutdown-only message does not justify deleting configuration or character/world saves.

[Direct link to RS-GAME-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-007)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-008" data-rs-severity="warning" markdown="1">

### RS-GAME-008: Native crowd navigation unavailable {#rs-game-008}

**Game navigation · Typical level: warning**

The native crowd manager could not find a RecastNavMesh instance.

**Common reasons**

The crowd manager did not find a RecastNavMesh in its current world. The world may be loading or already tearing down.

**What RuneSchema does**

Crowd navigation may be unavailable in the current world lifecycle stage. A teardown-only warning does not prove broken map content.

**What to do next**

Record the world and whether AI is affected during play. Compare with world initialization or teardown events.

**Troubleshooting checks**

Record the world and any AI navigation symptoms during play. Keep surrounding load/teardown events. Do not generate replacement navigation data or blame a mod without asset evidence.

[Direct link to RS-GAME-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-008)

</section>

<section class="rs-code-card" data-rs-code="RS-GAME-009" data-rs-severity="warning" markdown="1">

### RS-GAME-009: Native tagged-object cleanup mismatch {#rs-game-009}

**Game object lifecycle · Typical level: warning**

The native registry-tag subsystem could not unregister a tagged object as expected.

**Common reasons**

Native tagged-object registration and cleanup did not agree about an object's lifecycle.

**What RuneSchema does**

This concerns native gameplay-object tags, not RuneSchema persistence-ID declarations. It does not authorize save pruning.

**What to do next**

Preserve the exact object and lifecycle stage. Investigate repeated failures during play separately from shutdown-only messages.

**Troubleshooting checks**

Capture the full object path and preceding lifecycle messages. This is not the RuneSchema persistence-ID registry, and neither automatic pruning nor invented declarations will repair it.

[Direct link to RS-GAME-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-game-009)

</section>

## Items — RS-ITEM {#legacy-family-rs-item}

<section class="rs-code-card" data-rs-code="RS-ITEM-001" data-rs-severity="warning" markdown="1">

### RS-ITEM-001: Some items were not delivered {#rs-item-001}

**Items · Typical level: warning**

One or more items failed independent delivery validation.

**Common reasons**

Individual cart entries failed native validation or delivery while other entries may have succeeded.

**What RuneSchema does**

Successful deliveries are not repeated to compensate.

**What to do next**

Review cart results and retry only failed items after resolving their cause.

**Troubleshooting checks**

Read each placard's result and resolve only failed entries. Verify the intended recipient and availability. Never resend the entire cart to compensate for a partial failure.

[Direct link to RS-ITEM-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-item-001)

</section>

## Journal — RS-JRN {#legacy-family-rs-jrn}

<section class="rs-code-card" data-rs-code="RS-JRN-001" data-rs-severity="info" markdown="1">

### RS-JRN-001: Journal cache is an observed subset {#rs-jrn-001}

!!! note "Shared label: choose the namespace"
    This is the **legacy reference**. A structured report uses a different meaning: [structured RS-JRN-001](#structured-rs-jrn-001). Check the original source and format.

**Journal · Typical level: info**

Full-game journal coverage is not certified.

**Common reasons**

Only journal entries observed or explicitly declared in the bundle are cached.

**What RuneSchema does**

Unseen entries are not assumed absent or unlocked.

**What to do next**

Import a compatible journal bundle or observe additional native entries.

**Troubleshooting checks**

Check category coverage for World, Recipes and Knowledge. Import a compatible journal bundle or observe additional entries. Cached visibility does not grant journal unlocks.

[Direct link to RS-JRN-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-001)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-002" data-rs-severity="warning" markdown="1">

### RS-JRN-002: Journal cache could not be read {#rs-jrn-002}

!!! note "Shared label: choose the namespace"
    This is the **legacy reference**. A structured report uses a different meaning: [structured RS-JRN-002](#structured-rs-jrn-002). Check the original source and format.

**Journal · Typical level: warning**

The journal record format or text bounds failed validation.

**Common reasons**

A journal cache contains unsupported records or invalid text/page bounds.

**What RuneSchema does**

The invalid record set is not used.

**What to do next**

Import a supported bundle or refresh the native journal cache.

**Troubleshooting checks**

Keep the original save intact. Import a supported bundle or refresh the native journal cache, then inspect the original record error if it recurs.

[Direct link to RS-JRN-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-002)

</section>

## Console — RS-LOG {#legacy-family-rs-log}

<section class="rs-code-card" data-rs-code="RS-LOG-000" data-rs-severity="info" markdown="1">

### RS-LOG-000: Unclassified diagnostic {#rs-log-000}

**Console · Typical level: info**

This message has no registered exact diagnostic code.

**Common reasons**

An older or uncoded warning/error has no exact registered diagnosis. A symptom-based suggestion may accompany it.

**What RuneSchema does**

Original severity and raw text remain authoritative.

**What to do next**

Read the original subsystem message; a suggested reference is guidance, not a diagnosed cause.

**Troubleshooting checks**

Read and copy the original event and the preceding failures. Record the subsystem and versions. A suggested code is a troubleshooting lead, not proof of that cause; preserve unfamiliar code text exactly.

[Direct link to RS-LOG-000](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-log-000)

</section>

<section class="rs-code-card" data-rs-code="RS-LOG-001" data-rs-severity="warning" markdown="1">

### RS-LOG-001: File logging is unavailable {#rs-log-001}

**Console · Typical level: warning**

RuneSchema cannot open or safely archive its log.

**Common reasons**

The mod's log or archive directory cannot be opened, storage is full, or a previous log cannot be archived safely.

**What RuneSchema does**

File logging is disabled; UE4SS warnings and summaries remain available.

**What to do next**

Check folder permissions and free space; keep the previous log.

**Troubleshooting checks**

Check free space and write access to the active RuneSchema mod folder and logs directory. Preserve the previous log. Check the UE4SS warning for the original failure and retry on the next normal launch.

[Direct link to RS-LOG-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-log-001)

</section>

<section class="rs-code-card" data-rs-code="RS-LOG-002" data-rs-severity="warning" markdown="1">

### RS-LOG-002: Export destination is protected {#rs-log-002}

**Console · Typical level: warning**

An export would overwrite a live log or archive.

**Common reasons**

The selected export path resolves to a live source log or a protected RuneSchema archive.

**What RuneSchema does**

No export is written to that path.

**What to do next**

Choose another filename outside the active log and archive paths.

**Troubleshooting checks**

Export to a new filename in a separate directory. Check aliases/junctions if the displayed path looks different. Do not delete the live log to make the export succeed.

[Direct link to RS-LOG-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-log-002)

</section>

## Map — RS-MAP {#legacy-family-rs-map}

<section class="rs-code-card" data-rs-code="RS-MAP-001" data-rs-severity="warning" markdown="1">

### RS-MAP-001: Map target is not loaded {#rs-map-001}

**Map · Typical level: warning**

The selected ground, node or storage actor is unavailable.

**Common reasons**

The map reference exists but its live ground, resource node or storage actor is not currently loaded.

**What RuneSchema does**

Unavailable contents are not an empty chest or harvested node.

**What to do next**

Move closer and refresh that target.

**Troubleshooting checks**

Move within live observation range and refresh the target. A marker is a location reference, not proof of available resources or empty chest contents.

[Direct link to RS-MAP-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-map-001)

</section>

<section class="rs-code-card" data-rs-code="RS-MAP-002" data-rs-severity="info" markdown="1">

### RS-MAP-002: Resource respawn time is unverified {#rs-map-002}

**Map · Typical level: info**

The observation does not establish an exact countdown.

**Common reasons**

The live observation does not establish a native respawn deadline for this resource.

**What RuneSchema does**

A cached map marker never invents a timer.

**What to do next**

Observe a nearby live node; cached references only establish locations.

**Troubleshooting checks**

Observe the nearby node after harvesting and wait for fresh supported timer data. Keep the timer marked unknown until verified; do not start an invented countdown from a cached marker.

[Direct link to RS-MAP-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-map-002)

</section>

## Multiplayer — RS-NET {#legacy-family-rs-net}

<section class="rs-code-card" data-rs-code="RS-NET-001" data-rs-severity="error" markdown="1">

### RS-NET-001: Client and server content differ {#rs-net-001}

**Multiplayer · Typical level: error**

Verified registry fingerprints do not match.

**Common reasons**

The compared client/server registry identities differ. This reference does not mean every content difference rejects the entire session; the original message identifies the affected operation.

**What RuneSchema does**

Session policy determines the affected owner or operation; a content difference alone does not prove the whole session was rejected. This is not a latency measurement.

**What to do next**

Compare the named owner and protocol/build details; align required content before retrying the affected operation.

**Troubleshooting checks**

Compare the named owner and protocol/build information on host and client. Update that content on both sides and reconnect normally; do not treat latency tuning as a registry fix.

[Direct link to RS-NET-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-net-001)

</section>

<section class="rs-code-card" data-rs-code="RS-NET-002" data-rs-severity="warning" markdown="1">

### RS-NET-002: Authority registry negotiation warning {#rs-net-002}

**Multiplayer · Typical level: warning**

Client content or mapping availability differs from the authority.

**Common reasons**

The authority and client differ in content definitions, source membership, a protected registry baseline, or the availability of a verified mapping. Client-only content and version drift do not themselves cause a RuneSchema entry refusal.

**What RuneSchema does**

RuneSchema warns and preserves native entry; it only aligns shared IDs when identity and type checks succeed. Individual unsupported operations can remain unavailable.

**What to do next**

Read the named source or registry difference. Client-only mods do not need to be removed merely to enter; align required content to use unavailable server features.

**Troubleshooting checks**

Read the original source/lane detail. Keep extra client mods installed if desired. Shared IDs are aligned only when their identities and native types are verified; update required content for unavailable server features. A warning does not certify that the game itself can deserialize a missing cooked class.

[Direct link to RS-NET-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-net-002)

</section>

## Party — RS-PARTY {#legacy-family-rs-party}

<section class="rs-code-card" data-rs-code="RS-PARTY-001" data-rs-severity="info" markdown="1">

### RS-PARTY-001: Player reading is unavailable {#rs-party-001}

**Party · Typical level: info**

A reading is missing, stale or unsupported.

**Common reasons**

A player meter, skill or equipment reading is stale, missing, out of scope or unsupported by the native contract.

**What RuneSchema does**

It is not displayed as zero or claimed to be authoritative.

**What to do next**

Wait for fresh data and check support for that field.

**Troubleshooting checks**

Wait for fresh host/server data and reselect the player. Check which field is unavailable. Do not interpret missing data as dead, empty or zero.

[Direct link to RS-PARTY-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-party-001)

</section>

<section class="rs-code-card" data-rs-code="RS-PARTY-002" data-rs-severity="warning" markdown="1">

### RS-PARTY-002: Host authority is required {#rs-party-002}

**Party · Typical level: warning**

The caller is not authorized for the action.

**Common reasons**

The requester lacks current host/server authority for the selected action.

**What RuneSchema does**

The action is refused; a host badge grants no privilege by itself.

**What to do next**

Use the authorized host/server flow and current selected character.

**Troubleshooting checks**

Use the authorized host/server workflow. Confirm the selected player and permission settings; a visual host badge does not itself confer authority.

[Direct link to RS-PARTY-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-party-002)

</section>

<section class="rs-code-card" data-rs-code="RS-PARTY-003" data-rs-severity="warning" markdown="1">

### RS-PARTY-003: Player or world identity changed {#rs-party-003}

**Party · Typical level: warning**

The selected character, serial or world generation is stale.

**Common reasons**

The selected player/chest serial or world generation no longer identifies the original target.

**What RuneSchema does**

The action is not redirected to a different player or reused object.

**What to do next**

Refresh and select the current player or chest again.

**Troubleshooting checks**

Refresh and explicitly reselect the current target after reconnecting or changing worlds. Do not replay a stale action against a player with the same display name.

[Direct link to RS-PARTY-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-party-003)

</section>

## Performance — RS-PERF {#legacy-family-rs-perf}

<section class="rs-code-card" data-rs-code="RS-PERF-001" data-rs-severity="warning" markdown="1">

### RS-PERF-001: A runtime task took too long {#rs-perf-001}

**Performance · Typical level: warning**

A measured work slice exceeded its reporting threshold.

**Common reasons**

A measured game-thread work slice exceeded its threshold, often during discovery, refresh or a large content pass.

**What RuneSchema does**

This attributes slow work; it does not prove network rejection or save corruption.

**What to do next**

Inspect the phase and elapsed time; prefer compatible caches and incremental refresh.

**Troubleshooting checks**

Compare the named phase, elapsed_ms, peak_ms and slow_calls. Use a compatible bundle and incremental refresh first. Report repeatable slow phases with game/storefront versions; do not erase the entire cache as a first step.

[Direct link to RS-PERF-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-perf-001)

</section>

## Plugins — RS-PLUG {#legacy-family-rs-plug}

<section class="rs-code-card" data-rs-code="RS-PLUG-001" data-rs-severity="error" markdown="1">

### RS-PLUG-001: Plugin catalogue discovery failed {#rs-plug-001}

**Plugins · Typical level: error**

Plugin catalogue discovery threw an exception before loading could proceed.

**Common reasons**

The plugin discovery pass threw an exception. This is different from one plugin failing its native initialization.

**What RuneSchema does**

This plugin load pass stops; RuneSchema core continues.

**What to do next**

Inspect the original discovery exception, plugin manifests and directory access.

**Troubleshooting checks**

Keep the original exception and inspect plugin manifests and directory access. RuneSchema core continues; do not infer a game crash from this report.

[Direct link to RS-PLUG-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-001)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-002" data-rs-severity="warning" markdown="1">

### RS-PLUG-002: Declared plugin DLL is missing {#rs-plug-002}

**Plugins · Typical level: warning**

A plugin declares a native entry point that is not a regular file at the resolved path.

**Common reasons**

The manifest's resolved native entry point is absent or is not a regular file.

**What RuneSchema does**

The native plugin is skipped; its PAK content remains independently available.

**What to do next**

Check the named plugin's entry point and installation layout; restore the matching DLL.

**Troubleshooting checks**

Compare plugin.json with the installed DLL path. Restore the correct package without changing unrelated PAKs or UE4SS host files.

[Direct link to RS-PLUG-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-002)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-003" data-rs-severity="error" markdown="1">

### RS-PLUG-003: Windows could not load a plugin DLL {#rs-plug-003}

**Plugins · Typical level: error**

LoadLibraryExW returned failure for the named native plugin.

**Common reasons**

Windows rejected LoadLibraryExW. The retained Windows number may indicate missing dependencies, invalid architecture or access restrictions; the RS code does not select one cause.

**What RuneSchema does**

The native plugin is skipped; other plugins continue.

**What to do next**

Keep the reported Windows error number and DLL path. Check architecture, dependencies and access before replacing files.

**Troubleshooting checks**

Keep the exact Windows number, plugin ID and DLL path. Verify a matching x64 plugin build and its dependencies before reinstalling. Do not disable security software as a generic fix.

[Direct link to RS-PLUG-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-003)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-004" data-rs-severity="error" markdown="1">

### RS-PLUG-004: Plugin API exports are missing {#rs-plug-004}

**Plugins · Typical level: error**

A loaded DLL lacks a mandatory Query, Initialize or Shutdown export.

**Common reasons**

The DLL loaded but did not export all mandatory RuneSchema plugin API functions.

**What RuneSchema does**

The native plugin is unloaded before initialization.

**What to do next**

Install a RuneSchema plugin build with the required exports, not an arbitrary UE4SS DLL.

**Troubleshooting checks**

Check for RuneSchemaPlugin_Query, RuneSchemaPlugin_Initialize and RuneSchemaPlugin_Shutdown in the intended plugin build. A generic UE4SS main.dll is not interchangeable.

[Direct link to RS-PLUG-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-004)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-005" data-rs-severity="error" markdown="1">

### RS-PLUG-005: Plugin identity or API contract rejected {#rs-plug-005}

**Plugins · Typical level: error**

The queried native descriptor does not satisfy the host's identity/API contract.

**Common reasons**

The queried descriptor was absent, too small, had a different API version/ID or lacked required identity fields.

**What RuneSchema does**

The incompatible native plugin is isolated; core, other plugins and PAK mounting continue.

**What to do next**

Align the manifest ID and DLL descriptor/API build. A version label alone does not repair the descriptor.

**Troubleshooting checks**

Align the manifest and DLL descriptor with the running host. A manifest version mismatch alone is not this rejection; preserve the actual descriptor evidence.

[Direct link to RS-PLUG-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-005)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-006" data-rs-severity="error" markdown="1">

### RS-PLUG-006: Plugin initialization contract failed {#rs-plug-006}

**Plugins · Typical level: error**

Initialization failed or the plugin did not register its declared capabilities.

**Common reasons**

Initialize returned failure, threw, or did not register all capabilities declared by the manifest.

**What RuneSchema does**

Its registered services/capabilities are removed and the DLL is unloaded.

**What to do next**

Read earlier plugin errors and the native result; verify that all declared capabilities are registered.

**Troubleshooting checks**

Check preceding plugin errors and the result value. A successful return value can still fail capability verification; compare declared and registered capabilities.

[Direct link to RS-PLUG-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-006)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-007" data-rs-severity="error" markdown="1">

### RS-PLUG-007: Plugin UI initialization failed {#rs-plug-007}

**Plugins · Typical level: error**

A plugin UI callback rejected initialization or threw an exception.

**Common reasons**

The UI initialization callback returned failure or threw. This does not imply every plugin service has been unloaded.

**What RuneSchema does**

That UI phase did not complete; the message does not establish that all plugin services stopped.

**What to do next**

Keep the named plugin and rejection/exception message. Check matching frontend/core builds and earlier UI errors.

**Troubleshooting checks**

Keep the plugin name and callback outcome. Inspect earlier frontend/device/configuration errors and use a matching plugin build.

[Direct link to RS-PLUG-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-007)

</section>

<section class="rs-code-card" data-rs-code="RS-PLUG-008" data-rs-severity="error" markdown="1">

### RS-PLUG-008: Plugin Unreal initialization failed {#rs-plug-008}

**Plugins · Typical level: error**

A plugin Unreal initialization callback returned failure or threw an exception.

**Common reasons**

The Unreal initialization callback did not confirm success. An earlier plugin-specific error may explain why.

**What RuneSchema does**

The plugin did not confirm Unreal initialization; core and other plugin phases continue.

**What to do next**

Inspect the named plugin's preceding error and reported result, rather than treating this phase failure as a complete diagnosis.

**Troubleshooting checks**

Read the earlier plugin messages and returned result. Do not treat this wrapper failure as a missing-asset or migration diagnosis without that evidence.

[Direct link to RS-PLUG-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-plug-008)

</section>

## Quests — RS-QST {#legacy-family-rs-qst}

<section class="rs-code-card" data-rs-code="RS-QST-001" data-rs-severity="error" markdown="1">

### RS-QST-001: Conflicting quest ownership {#rs-qst-001}

**Quests · Typical level: error**

Multiple definitions claim one quest identity.

**Common reasons**

Two mods or duplicate files declare the same quest identity; an old and new release may both be enabled.

**What RuneSchema does**

The conflicting definition cannot be safely enrolled.

**What to do next**

Keep one owner for that ID; review the named mod files.

**Troubleshooting checks**

Find the named owners and declaration files in the original event. Disable only the duplicate release or ask the authors to resolve ownership; do not rename persistence IDs in an existing save.

[Direct link to RS-QST-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-qst-001)

</section>

## Registry — RS-REG {#legacy-family-rs-reg}

<section class="rs-code-card" data-rs-code="RS-REG-001" data-rs-severity="warning" markdown="1">

### RS-REG-001: Registry enrollment is not ready {#rs-reg-001}

**Registry · Typical level: warning**

The complete mounted registry is not certified yet.

**Common reasons**

Startup enrollment is still running, a content module has not registered, or a declaration was rejected earlier.

**What RuneSchema does**

Automatic cleanup stays disabled; verified content can still run.

**What to do next**

Wait for registration, then check framework and content versions.

**Troubleshooting checks**

Wait for startup to finish, then locate the first registry rejection before this message. Check the matching mod, framework and game versions. Do not enable pruning to force readiness.

[Direct link to RS-REG-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-reg-001)

</section>

## Runtime — RS-RT {#legacy-family-rs-rt}

<section class="rs-code-card" data-rs-code="RS-RT-001" data-rs-severity="error" markdown="1">

### RS-RT-001: Native API contract changed {#rs-rt-001}

**Runtime · Typical level: error**

A reflected function, field or parameter layout is unsupported.

**Common reasons**

The game/storefront build exposes a reflected function or field layout that this operation does not support.

**What RuneSchema does**

That operation stops; arbitrary memory offsets are not substituted.

**What to do next**

Record the game build, storefront and framework version and update compatible components.

**Troubleshooting checks**

Record the exact function/field, game build, Steam or Game Pass, UE4SS and RuneSchema versions. Install compatible components. Do not substitute guessed memory offsets.

[Direct link to RS-RT-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rt-001)

</section>

<section class="rs-code-card" data-rs-code="RS-RT-002" data-rs-severity="info" markdown="1">

### RS-RT-002: Runtime is becoming ready {#rs-rt-002}

**Runtime · Typical level: info**

The service is not ready in this world generation.

**Common reasons**

The world, player or service is still starting, changing worlds or becoming available.

**What RuneSchema does**

Work waits or returns unavailable, not a fabricated zero.

**What to do next**

Wait for the world to become ready, then retry the specific operation.

**Troubleshooting checks**

Wait for the current world to become ready and retry only the affected action. If it persists after loading, capture the first readiness/contract failure. An unavailable meter is not a zero reading.

[Direct link to RS-RT-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rt-002)

</section>

## Save safety — RS-SAVE {#legacy-family-rs-save}

<section class="rs-code-card" data-rs-code="RS-SAVE-001" data-rs-severity="warning" markdown="1">

### RS-SAVE-001: Original save is protected {#rs-save-001}

**Save safety · Typical level: warning**

The operation targets a protected original or active save.

**Common reasons**

The operation targets a protected original save or a character/world that is still active.

**What RuneSchema does**

No unreviewed original-save change is performed.

**What to do next**

Use the copy-only editor and review its export, or close the relevant world normally.

**Troubleshooting checks**

Use an editable copy, close the relevant world normally when required, and review the export diff. Export to a new path; never force an active-save overwrite.

[Direct link to RS-SAVE-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-save-001)

</section>

<section class="rs-code-card" data-rs-code="RS-SAVE-002" data-rs-severity="warning" markdown="1">

### RS-SAVE-002: Persistence cleanup is not certified {#rs-save-002}

**Save safety · Typical level: warning**

Complete stable registry evidence is unavailable.

**Common reasons**

The applicable stable registry evidence is unavailable for one or more sections; incomplete loading must not be mistaken for removed content.

**What RuneSchema does**

Uncertified sections stay unchanged. Other independently certified sections may still be cleaned; native loading continues.

**What to do next**

Resolve readiness first; a missing lookup is not proof of removed content.

**Troubleshooting checks**

Review the SECTION-SKIPPED entries. Certified sections continue independently; unresolved sections are retained. Native loading still runs, using relaxed mode and rejection fallback where enabled.

[Direct link to RS-SAVE-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-save-002)

</section>

## Spawns — RS-SPAWN {#legacy-family-rs-spawn}

<section class="rs-code-card" data-rs-code="RS-SPAWN-001" data-rs-severity="warning" markdown="1">

### RS-SPAWN-001: Placement needs loaded ground {#rs-spawn-001}

!!! note "Shared label: choose the namespace"
    This is the **legacy reference**. A structured report uses a different meaning: [structured RS-SPAWN-001](#structured-rs-spawn-001). Check the original source and format.

**Spawns · Typical level: warning**

Ground or collision is not confirmed at the target.

**Common reasons**

The destination is outside loaded collision, or the placement footprint cannot be confirmed.

**What RuneSchema does**

Placement is deferred or refused, not placed at a guessed height.

**What to do next**

Move closer to load the area or choose another point.

**Troubleshooting checks**

Move closer to load the target area and retry at a visible ground point. Review slope/footprint details. Do not bypass collision validation or assume the cached map provides ground height.

[Direct link to RS-SPAWN-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-001)

</section>

## Content — RS-SRC {#legacy-family-rs-src}

<section class="rs-code-card" data-rs-code="RS-SRC-001" data-rs-severity="warning" markdown="1">

### RS-SRC-001: Content source is not mounted {#rs-src-001}

**Content · Typical level: warning**

A required content source is absent or unregistered.

**Common reasons**

The source mod is absent, disabled, unmounted or has not completed registration.

**What RuneSchema does**

Its cache cannot create gameplay definitions or prove availability.

**What to do next**

Mount matching content or keep its references marked unavailable.

**Troubleshooting checks**

Enable/install the matching content and let registration finish, or leave its references unavailable. Downloading its pictures/cache alone cannot create its gameplay definitions.

[Direct link to RS-SRC-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-src-001)

</section>

## Server — RS-SRV {#legacy-family-rs-srv}

<section class="rs-code-card" data-rs-code="RS-SRV-001" data-rs-severity="info" markdown="1">

### RS-SRV-001: Native server filter setup requested {#rs-srv-001}

**Server · Typical level: info**

RuneSchema applied the server's existing accessibility filter preferences to its actual world.

**Common reasons**

The dedicated server reached a real world and RuneSchema invoked the game's native accessibility-filter setup with its existing preferences. The client-only settings path can otherwise leave this subsystem uninitialized.

**What RuneSchema does**

The game's native filter can finish initialization before processing player names. No filter preference is changed; native loading completes asynchronously.

**What to do next**

No action is normally needed. This records the setup request, not a complete server-health check.

**Troubleshooting checks**

Native dictionary loading is asynchronous. Preserve any later native filter warnings when reporting an issue; this information code alone does not certify filtering, a player join or shutdown.

[Direct link to RS-SRV-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-srv-001)

</section>

<section class="rs-code-card" data-rs-code="RS-SRV-002" data-rs-severity="warning" markdown="1">

### RS-SRV-002: Native server filter setup unavailable {#rs-srv-002}

**Server · Typical level: warning**

The dedicated-server executable, native method or required settings/world reference could not be verified.

**Common reasons**

The correction could not verify the exact server executable/method, install its world-ready callback, or obtain the required native settings and game-instance references.

**What RuneSchema does**

RuneSchema skips its initialization correction and retains the game's handling. The native filter setup/shutdown wait can still occur.

**What to do next**

Keep the original reason and game version. Use a RuneSchema build verified for that server executable; do not disable filtering to conceal the wait.

**Troubleshooting checks**

Retain the full reason and executable/game version. Unsupported builds keep native handling and need a verified compatibility update; do not patch flags or turn filtering off as a generic workaround.

[Direct link to RS-SRV-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-srv-002)

</section>

## Equipment — RS-WEAR {#legacy-family-rs-wear}

<section class="rs-code-card" data-rs-code="RS-WEAR-001" data-rs-severity="warning" markdown="1">

### RS-WEAR-001: Wearable contract is not enrolled {#rs-wear-001}

**Equipment · Typical level: warning**

The native wearable contract is not certified.

**Common reasons**

A wearable declaration is missing its supported native slot contract, or its enrollment was rejected.

**What RuneSchema does**

Unsupported wearable work is skipped rather than guessed.

**What to do next**

Check the equipment definition and native slot contract.

**Troubleshooting checks**

Check the named equipment definition and earlier enrollment errors. Verify its slot and cooked class against the current game version; leave unsupported equipment unequipped.

[Direct link to RS-WEAR-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-wear-001)

</section>

## World save cleanup — RS-WORLD {#legacy-family-rs-world}

<section class="rs-code-card" data-rs-code="RS-WORLD-001" data-rs-severity="warning" markdown="1">

### RS-WORLD-001: World cleanup buffer changed during validation {#rs-world-001}

**World save cleanup · Typical level: warning**

The native world buffer no longer matched the validated cleanup input.

**Common reasons**

The native buffer pointer/length/content no longer matched the prevalidated cleanup input at commit time.

**What RuneSchema does**

Cleanup skips the buffer; the world is unchanged by this attempt.

**What to do next**

Keep the original world and backup. Check lifecycle/overlapping work before retrying; do not force stale bytes into the save.

**Troubleshooting checks**

Keep the original save and backup. Investigate overlapping native loading or lifecycle changes. Never bypass the comparison to force a stale cleanup result.

[Direct link to RS-WORLD-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-world-001)

</section>

<section class="rs-code-card" data-rs-code="RS-WORLD-002" data-rs-severity="warning" markdown="1">

### RS-WORLD-002: World cleanup contract is not safe {#rs-world-002}

**World save cleanup · Typical level: warning**

Missing building IDs were observed but the native load contract was not safe to repair.

**Common reasons**

Orphan IDs were detected but the verified native world-load repair contract was not satisfied.

**What RuneSchema does**

The cleanup attempt leaves the world unchanged.

**What to do next**

Check the preceding contract and registry evidence. Missing IDs alone do not authorize writing an unverified save format.

**Troubleshooting checks**

Review the preceding contract/binding and registry evidence for this exact game build. Do not disable save validation merely to silence this warning.

[Direct link to RS-WORLD-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-world-002)

</section>

<section class="rs-code-card" data-rs-code="RS-WORLD-003" data-rs-severity="warning" markdown="1">

### RS-WORLD-003: World cleanup repair did not complete {#rs-world-003}

**World save cleanup · Typical level: warning**

The native world-buffer repair attempt reported an exception.

**Common reasons**

A world-buffer repair stage reported an exception before its cleanup could be accepted.

**What RuneSchema does**

The cleanup attempt reports the world unchanged; the original exception remains authoritative.

**What to do next**

Retain the original world, backups and full error. Resolve the reported validation/repair cause before retrying.

**Troubleshooting checks**

Keep the original exception and backup path. Resolve the named validation or repair condition; this code does not claim the native game save is corrupt.

[Direct link to RS-WORLD-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-world-003)

</section>

<section class="rs-code-card" data-rs-code="RS-WORLD-004" data-rs-severity="warning" markdown="1">

### RS-WORLD-004: World cleanup input could not be validated {#rs-world-004}

**World save cleanup · Typical level: warning**

World data parsing or validation failed before a cleanup result could be accepted.

**Common reasons**

Native world data could not be parsed or validated by this cleanup adapter.

**What RuneSchema does**

This cleanup path does not write a guessed repair.

**What to do next**

Keep the exact parser/validation exception and game build. A cleanup rejection alone does not prove the native game cannot load the world.

**Troubleshooting checks**

Preserve the original data and exact exception. Compare the adapter with the game build; native loading can still proceed without an invented repair.

[Direct link to RS-WORLD-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-world-004)

</section>

<section class="rs-code-card" data-rs-code="RS-WORLD-005" data-rs-severity="warning" markdown="1">

### RS-WORLD-005: Game Pass world cleanup hooks unavailable {#rs-world-005}

**World save cleanup · Typical level: warning**

Required Game Pass world-loading hooks could not be installed.

**Common reasons**

One or more required Game Pass world-loading hooks were unavailable for the current executable.

**What RuneSchema does**

That world cleanup path stays unavailable and does not change saves.

**What to do next**

Check the current Game Pass executable/build and matching RuneSchema bindings; preserve existing saves and use supported native loading.

**Troubleshooting checks**

Use a matching RuneSchema build/binding and keep the original save. Do not install a different UE4SS host just to suppress this framework cleanup warning.

[Direct link to RS-WORLD-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-world-005)

</section>

## Structured diagnostic codes

These entries come from the producer registry used by the current diagnostics build. They are distinct from older symptom/reference codes above. Producer paths identify the reporting subsystem, not a guarantee that the user's file has that path.

## Assets — structured RS-ASSET {#structured-family-rs-asset}

<section class="rs-code-card" data-rs-code="RS-ASSET-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-ASSET-001: Asset target remained unresolved {#rs-asset-001}

**Structured namespace · Typical level: error**

**Condition:** Asset target remained unresolved. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Assets · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Check the exact named target and whether its content is mounted or loaded. An unresolved lookup does not, by itself, prove a misspelled path.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-ASSET-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-asset-001)

</section>

<section class="rs-code-card" data-rs-code="RS-ASSET-002" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-ASSET-002: Asset definition has invalid root {#rs-asset-002}

**Structured namespace · Typical level: warning**

**Condition:** Asset definition has invalid root. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Assets · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

The asset definition root was rejected. Review the file’s top-level JSONC type and structure.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-ASSET-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-asset-002)

</section>

<section class="rs-code-card" data-rs-code="RS-ASSET-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-ASSET-003: Asset property modification rejected {#rs-asset-003}

**Structured namespace · Typical level: warning**

**Condition:** Asset property modification rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Assets · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

The requested asset field could not be applied. Inspect the exact field path and original rejection; other fields may have succeeded.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-ASSET-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-asset-003)

</section>

## Buildings — structured RS-BLD {#structured-family-rs-bld}

<section class="rs-code-card" data-rs-code="RS-BLD-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-001: Invalid building document root {#rs-bld-001}

**Structured namespace · Typical level: error**

**Condition:** Invalid building document root. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-001)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-002: Invalid building definition {#rs-bld-002}

**Structured namespace · Typical level: error**

**Condition:** Invalid building definition. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

The named building entry must be an object. Review that entry in the identified building definition file.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-002)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-003: Invalid building asset reference {#rs-bld-003}

**Structured namespace · Typical level: error**

**Condition:** Invalid building asset reference. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-003)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-004: Invalid building properties {#rs-bld-004}

**Structured namespace · Typical level: error**

**Condition:** Invalid building properties. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

The building Properties value must be an object. Check the named definition rather than changing unrelated building files.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-004)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-005: Building patch target missing {#rs-bld-005}

**Structured namespace · Typical level: error**

**Condition:** Building patch target missing. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

A building patch could not find its target. Confirm the exact existing building identity and whether that definition is loaded.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-005)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-006: Invalid building patch declaration {#rs-bld-006}

**Structured namespace · Typical level: error**

**Condition:** Invalid building patch declaration. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-006)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-007: Invalid building requirements {#rs-bld-007}

**Structured namespace · Typical level: error**

**Condition:** Invalid building requirements. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Building Requirements must be an array. Check the failing field in the named definition.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-007)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-008: Protected building property {#rs-bld-008}

**Structured namespace · Typical level: error**

**Condition:** Protected building property. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

The building patch attempted a protected managed identity change. Leave the managed identity unchanged.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-008)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-009: Invalid BuildableActor reference {#rs-bld-009}

**Structured namespace · Typical level: error**

**Condition:** Invalid BuildableActor reference. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-009)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-010: Invalid requirement entry {#rs-bld-010}

**Structured namespace · Typical level: error**

**Condition:** Invalid requirement entry. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-010)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-011: Unsupported building override {#rs-bld-011}

**Structured namespace · Typical level: error**

**Condition:** Unsupported building override. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-011)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-012" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-012: Invalid building unlock {#rs-bld-012}

**Structured namespace · Typical level: error**

**Condition:** Invalid building unlock. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-012)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-013" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-013: Invalid building collection placement {#rs-bld-013}

**Structured namespace · Typical level: error**

**Condition:** Invalid building collection placement. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-013)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-014" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-014: Missing building pieces array {#rs-bld-014}

**Structured namespace · Typical level: error**

**Condition:** Missing building pieces array. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

The Base Builder import must contain a pieces array. Check the reported import source and shape.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-014](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-014)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-015" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-015: Building piece limit exceeded {#rs-bld-015}

**Structured namespace · Typical level: error**

**Condition:** Building piece limit exceeded. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-015](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-015)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-016" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-016: Building world assets not ready {#rs-bld-016}

**Structured namespace · Typical level: error**

**Condition:** Building world assets not ready. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-016](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-016)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-017" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-017: Native building registry unavailable {#rs-bld-017}

**Structured namespace · Typical level: error**

**Condition:** Native building registry unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-017](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-017)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-018" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-018: Building baseline restoration failed {#rs-bld-018}

**Structured namespace · Typical level: error**

**Condition:** Building baseline restoration failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-018](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-018)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-019" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-019: Native building registry incomplete {#rs-bld-019}

**Structured namespace · Typical level: error**

**Condition:** Native building registry incomplete. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-019](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-019)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-020" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-020: Building world registration aborted {#rs-bld-020}

**Structured namespace · Typical level: error**

**Condition:** Building world registration aborted. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-020](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-020)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-021" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-021: Building registry rollback failed {#rs-bld-021}

**Structured namespace · Typical level: error**

**Condition:** Building registry rollback failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-021](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-021)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-022" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BLD-022: Building world recovery timed out {#rs-bld-022}

**Structured namespace · Typical level: warning**

**Condition:** Building world recovery timed out. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-022](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-022)

</section>

<section class="rs-code-card" data-rs-code="RS-BLD-023" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BLD-023: Cooked building enrollment rejected {#rs-bld-023}

**Structured namespace · Typical level: error**

**Condition:** Cooked building enrollment rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Buildings · **Producer:** `src/Loader/DragonWildsBuildingModLoader.cpp`

**What to check**

Check the exact cooked BuildingPieceData path, native PersistenceID/InternalName and optional assertions. Keep gameplay in the cooked asset; only AddTo and Unlock are enrollment metadata. World cleanup remains deferred for rejected content; unrelated character lanes can continue.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BLD-023](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bld-023)

</section>

## Blueprints — structured RS-BP {#structured-family-rs-bp}

<section class="rs-code-card" data-rs-code="RS-BP-001" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-001: Blueprint property not found {#rs-bp-001}

**Structured namespace · Typical level: warning**

**Condition:** Blueprint property not found. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Compare the complete property path with the target Blueprint's current reflected properties and nested structure.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-001)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BP-002: Required Blueprint lifecycle hook unavailable {#rs-bp-002}

**Structured namespace · Typical level: error**

**Condition:** Required Blueprint lifecycle hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

A required Blueprint lifecycle hook was unavailable. Review the reported hook and native build compatibility rather than assuming a property typo.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-002)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-003: Blueprint changes require restart {#rs-bp-003}

**Structured namespace · Typical level: warning**

**Condition:** Blueprint changes require restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

This Blueprint change was deferred by the live-reload path. Restart the game to check whether it can apply during startup.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-003)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-004" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-004: Blueprint world-teardown cleanup hook unavailable {#rs-bp-004}

**Structured namespace · Typical level: warning**

**Condition:** Blueprint world-teardown cleanup hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-004)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BP-005: Blueprint PostLoad vtable unavailable {#rs-bp-005}

**Structured namespace · Typical level: error**

**Condition:** Blueprint PostLoad vtable unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-005)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BP-006: Actor PostInitComponents vtable unavailable {#rs-bp-006}

**Structured namespace · Typical level: error**

**Condition:** Actor PostInitComponents vtable unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-006)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-007" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-007: Runtime widget rule failed {#rs-bp-007}

**Structured namespace · Typical level: warning**

**Condition:** Runtime widget rule failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-007)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-008" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-008: Runtime UI rule failed {#rs-bp-008}

**Structured namespace · Typical level: warning**

**Condition:** Runtime UI rule failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-008)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-BP-009: Blueprint modification failed {#rs-bp-009}

**Structured namespace · Typical level: error**

**Condition:** Blueprint modification failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-009)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-010" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-010: Blueprint component template unavailable {#rs-bp-010}

**Structured namespace · Typical level: warning**

**Condition:** Blueprint component template unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-010)

</section>

<section class="rs-code-card" data-rs-code="RS-BP-011" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-BP-011: Blueprint component definition invalid {#rs-bp-011}

**Structured namespace · Typical level: warning**

**Condition:** Blueprint component definition invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Blueprints · **Producer:** `src/Loader/DragonWildsBlueprintModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-BP-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-bp-011)

</section>

## Runtime — structured RS-CORE {#structured-family-rs-core}

<section class="rs-code-card" data-rs-code="RS-CORE-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-001: RuneSchema startup aborted {#rs-core-001}

**Structured namespace · Typical level: error**

**Condition:** RuneSchema startup aborted. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-001)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-002: GameInstance vtable unavailable {#rs-core-002}

**Structured namespace · Typical level: error**

**Condition:** GameInstance vtable unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-002)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-003: GameInstance initialization hook unavailable {#rs-core-003}

**Structured namespace · Typical level: error**

**Condition:** GameInstance initialization hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-003)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-004" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-004: Early DataTable observation unavailable {#rs-core-004}

**Structured namespace · Typical level: warning**

**Condition:** Early DataTable observation unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-004)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-005" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-005: Early DataTable hook unavailable {#rs-core-005}

**Structured namespace · Typical level: warning**

**Condition:** Early DataTable hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-005)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-006: Dedicated-server registry boundary unavailable {#rs-core-006}

**Structured namespace · Typical level: error**

**Condition:** Dedicated-server registry boundary unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-006)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-007: Core startup fallback unavailable {#rs-core-007}

**Structured namespace · Typical level: error**

**Condition:** Core startup fallback unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-007)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-008: Additional PAK directory hook unavailable {#rs-core-008}

**Structured namespace · Typical level: error**

**Condition:** Additional PAK directory hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-008)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-009: Additional PAK directory binding unavailable {#rs-core-009}

**Structured namespace · Typical level: error**

**Condition:** Additional PAK directory binding unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-009)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-010: Automatic reload callback unavailable {#rs-core-010}

**Structured namespace · Typical level: error**

**Condition:** Automatic reload callback unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-010)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-011: Mod auto-reload failed {#rs-core-011}

**Structured namespace · Typical level: error**

**Condition:** Mod auto-reload failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-011)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-012" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-012: Building world registration disabled {#rs-core-012}

**Structured namespace · Typical level: error**

**Condition:** Building world registration disabled. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-012)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-013" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-013: Pending string application failed {#rs-core-013}

**Structured namespace · Typical level: error**

**Condition:** Pending string application failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-013)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-014" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-014: Data registrar unavailable {#rs-core-014}

**Structured namespace · Typical level: error**

**Condition:** Data registrar unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-014](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-014)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-015" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-015: Networking registry bridge unavailable {#rs-core-015}

**Structured namespace · Typical level: error**

**Condition:** Networking registry bridge unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-015](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-015)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-016" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-016: Additional PAK mounting unavailable {#rs-core-016}

**Structured namespace · Typical level: error**

**Condition:** Additional PAK mounting unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-016](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-016)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-017" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-017: Plugin PAK order rejected {#rs-core-017}

**Structured namespace · Typical level: error**

**Condition:** Plugin PAK order rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-017](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-017)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-018" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-018: Mod PAK order rejected {#rs-core-018}

**Structured namespace · Typical level: error**

**Condition:** Mod PAK order rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-018](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-018)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-019" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-019: Loader initialization exception {#rs-core-019}

**Structured namespace · Typical level: warning**

**Condition:** Loader initialization exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-019](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-019)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-020" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-020: Loader finalization exception {#rs-core-020}

**Structured namespace · Typical level: warning**

**Condition:** Loader finalization exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-020](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-020)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-021" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-021: Recipe reference preparation failed {#rs-core-021}

**Structured namespace · Typical level: warning**

**Condition:** Recipe reference preparation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-021](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-021)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-022" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-022: Definition finalization exception {#rs-core-022}

**Structured namespace · Typical level: warning**

**Condition:** Definition finalization exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-022](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-022)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-023" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-023: Player appearance source skipped {#rs-core-023}

**Structured namespace · Typical level: warning**

**Condition:** Player appearance source skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-023](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-023)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-024" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-024: Mod metadata or discovery failed {#rs-core-024}

**Structured namespace · Typical level: warning**

**Condition:** Mod metadata or discovery failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-024](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-024)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-025" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-025: Nameplate section skipped {#rs-core-025}

**Structured namespace · Typical level: warning**

**Condition:** Nameplate section skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-025](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-025)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-026" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-026: Nameplate finalization exception {#rs-core-026}

**Structured namespace · Typical level: warning**

**Condition:** Nameplate finalization exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-026](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-026)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-027" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-027: Player rules section skipped {#rs-core-027}

**Structured namespace · Typical level: warning**

**Condition:** Player rules section skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-027](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-027)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-028" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-028: Player rules finalization exception {#rs-core-028}

**Structured namespace · Typical level: warning**

**Condition:** Player rules finalization exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-028](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-028)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-029" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-029: Optional persistence ledger unavailable {#rs-core-029}

**Structured namespace · Typical level: warning**

**Condition:** Optional persistence ledger unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/DragonWildsMainLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-029](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-029)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-030" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-030: UE4SS package preparation interrupted {#rs-core-030}

**Structured namespace · Typical level: warning**

**Condition:** UE4SS package preparation interrupted. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/dllmain.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-030](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-030)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-031" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-031: BP Mod Loader integration skipped {#rs-core-031}

**Structured namespace · Typical level: warning**

**Condition:** BP Mod Loader integration skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/dllmain.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-031](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-031)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-032" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-032: HELPY service validation failed {#rs-core-032}

**Structured namespace · Typical level: warning**

**Condition:** HELPY service validation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/dllmain.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-032](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-032)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-033" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-033: Quest registry preparation failed {#rs-core-033}

**Structured namespace · Typical level: error**

**Condition:** Quest registry preparation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsQuestLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-033](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-033)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-034" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-034: World quest registry preparation failed {#rs-core-034}

**Structured namespace · Typical level: error**

**Condition:** World quest registry preparation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsQuestLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-034](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-034)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-035" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-035: Quest reload requires restart {#rs-core-035}

**Structured namespace · Typical level: warning**

**Condition:** Quest reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsQuestLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-035](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-035)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-036" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-036: Quest initialization hook unavailable {#rs-core-036}

**Structured namespace · Typical level: error**

**Condition:** Quest initialization hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsQuestLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-036](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-036)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-037" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-037: Dialogue reload requires restart {#rs-core-037}

**Structured namespace · Typical level: warning**

**Condition:** Dialogue reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsDialogueLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-037](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-037)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-038" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-038: Native event hooks unavailable {#rs-core-038}

**Structured namespace · Typical level: error**

**Condition:** Native event hooks unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsEventLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-038](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-038)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-039" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-039: Event reload requires restart {#rs-core-039}

**Structured namespace · Typical level: warning**

**Condition:** Event reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `include/Loader/DragonWildsEventLoader.h`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-039](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-039)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-040" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-040: Load-order save refused {#rs-core-040}

**Structured namespace · Typical level: warning**

**Condition:** Load-order save refused. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/ModLoadOrder.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-040](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-040)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-041" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-041: Load-order save failed {#rs-core-041}

**Structured namespace · Typical level: error**

**Condition:** Load-order save failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/ModLoadOrder.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-041](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-041)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-042" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-042: Player definition file rejected {#rs-core-042}

**Structured namespace · Typical level: error**

**Condition:** Player definition file rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-042](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-042)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-043" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-043: Nameplate definition file rejected {#rs-core-043}

**Structured namespace · Typical level: error**

**Condition:** Nameplate definition file rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-043](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-043)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-044" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-044: Nameplate root invalid {#rs-core-044}

**Structured namespace · Typical level: error**

**Condition:** Nameplate root invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-044](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-044)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-045" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-045: Nameplate entry rejected {#rs-core-045}

**Structured namespace · Typical level: error**

**Condition:** Nameplate entry rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-045](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-045)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-046" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-046: Player definitions root invalid {#rs-core-046}

**Structured namespace · Typical level: error**

**Condition:** Player definitions root invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-046](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-046)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-047" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-047: Player rule entry rejected {#rs-core-047}

**Structured namespace · Typical level: error**

**Condition:** Player rule entry rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-047](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-047)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-048" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-048: Player patch target missing {#rs-core-048}

**Structured namespace · Typical level: error**

**Condition:** Player patch target missing. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-048](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-048)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-049" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CORE-049: Player rule registration failed {#rs-core-049}

**Structured namespace · Typical level: error**

**Condition:** Player rule registration failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Loader/Spawn/PlayerRules.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-049](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-049)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-050" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-050: Registry bridge attachment expired {#rs-core-050}

**Structured namespace · Typical level: warning**

**Condition:** Registry bridge attachment expired. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-050](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-050)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-051" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-051: Registry bridge attachment failed {#rs-core-051}

**Structured namespace · Typical level: warning**

**Condition:** Registry bridge attachment failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-051](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-051)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-052" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-052: Registry transport request rejected {#rs-core-052}

**Structured namespace · Typical level: warning**

**Condition:** Registry transport request rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-052](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-052)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-053" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-053: Registry bridge update failed {#rs-core-053}

**Structured namespace · Typical level: warning**

**Condition:** Registry bridge update failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-053](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-053)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-054" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-054: Registry bridge hooks unavailable {#rs-core-054}

**Structured namespace · Typical level: warning**

**Condition:** Registry bridge hooks unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-054](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-054)

</section>

<section class="rs-code-card" data-rs-code="RS-CORE-055" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CORE-055: Registry bridge startup catch-up failed {#rs-core-055}

**Structured namespace · Typical level: warning**

**Condition:** Registry bridge startup catch-up failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Runtime · **Producer:** `src/Runtime/RegistryBridge.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CORE-055](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-core-055)

</section>

## Courses — structured RS-CRS {#structured-family-rs-crs}

<section class="rs-code-card" data-rs-code="RS-CRS-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-001: Course definition rejected {#rs-crs-001}

**Structured namespace · Typical level: error**

**Condition:** Course definition rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-001)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-002: Invalid course patch {#rs-crs-002}

**Structured namespace · Typical level: error**

**Condition:** Invalid course patch. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-002)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-003: Course patch target unavailable {#rs-crs-003}

**Structured namespace · Typical level: error**

**Condition:** Course patch target unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

The course patch declaration was missing or malformed. Inspect the named patch identity and source structure.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-003)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-004: Course lifecycle hook unavailable {#rs-crs-004}

**Structured namespace · Typical level: error**

**Condition:** Course lifecycle hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-004)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-005" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CRS-005: Course reload requires restart {#rs-crs-005}

**Structured namespace · Typical level: warning**

**Condition:** Course reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-005)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-006: Course construction failed {#rs-crs-006}

**Structured namespace · Typical level: error**

**Condition:** Course construction failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-006)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-007: Course patch target malformed {#rs-crs-007}

**Structured namespace · Typical level: error**

**Condition:** Course patch target malformed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-007)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-008: Course patch target ambiguous {#rs-crs-008}

**Structured namespace · Typical level: error**

**Condition:** Course patch target ambiguous. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-008)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-009: Course prop lifecycle observer unavailable {#rs-crs-009}

**Structured namespace · Typical level: error**

**Condition:** Course prop lifecycle observer unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-009)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-010: Course build observer unavailable {#rs-crs-010}

**Structured namespace · Typical level: error**

**Condition:** Course build observer unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-010)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-CRS-011: Required agility content unavailable {#rs-crs-011}

**Structured namespace · Typical level: error**

**Condition:** Required agility content unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-011)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-012" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CRS-012: Course reward target unavailable {#rs-crs-012}

**Structured namespace · Typical level: warning**

**Condition:** Course reward target unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-012)

</section>

<section class="rs-code-card" data-rs-code="RS-CRS-013" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-CRS-013: Course prop class unusable {#rs-crs-013}

**Structured namespace · Typical level: warning**

**Condition:** Course prop class unusable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Courses · **Producer:** `src/Loader/DragonWildsCourseLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-CRS-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-crs-013)

</section>

## Enums — structured RS-ENUM {#structured-family-rs-enum}

<section class="rs-code-card" data-rs-code="RS-ENUM-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-ENUM-001: Enum class unavailable {#rs-enum-001}

**Structured namespace · Typical level: error**

**Condition:** Enum class unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Enums · **Producer:** `src/Loader/DragonWildsEnumLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-ENUM-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-enum-001)

</section>

<section class="rs-code-card" data-rs-code="RS-ENUM-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-ENUM-002: Enum declaration rejected {#rs-enum-002}

**Structured namespace · Typical level: error**

**Condition:** Enum declaration rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Enums · **Producer:** `src/Loader/DragonWildsEnumLoader.cpp`

**What to check**

The enum namespace or value is invalid for this loader. Check the exact namespace and enum entry in the source definition.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-ENUM-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-enum-002)

</section>

## Equipment — structured RS-EQP {#structured-family-rs-eqp}

<section class="rs-code-card" data-rs-code="RS-EQP-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-EQP-001: Equipment target unavailable {#rs-eqp-001}

**Structured namespace · Typical level: error**

**Condition:** Equipment target unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-001)

</section>

<section class="rs-code-card" data-rs-code="RS-EQP-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-EQP-002: Equipment effect rule rejected {#rs-eqp-002}

**Structured namespace · Typical level: error**

**Condition:** Equipment effect rule rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-002)

</section>

<section class="rs-code-card" data-rs-code="RS-EQP-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-EQP-003: Equipment field rule rejected {#rs-eqp-003}

**Structured namespace · Typical level: error**

**Condition:** Equipment field rule rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-003)

</section>

<section class="rs-code-card" data-rs-code="RS-EQP-004" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-EQP-004: Native equipment capability unavailable {#rs-eqp-004}

**Structured namespace · Typical level: warning**

**Condition:** Native equipment capability unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-004)

</section>

<section class="rs-code-card" data-rs-code="RS-EQP-005" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-EQP-005: Equipment behavior reload deferred {#rs-eqp-005}

**Structured namespace · Typical level: warning**

**Condition:** Equipment behavior reload deferred. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-005)

</section>

<section class="rs-code-card" data-rs-code="RS-EQP-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-EQP-006: Equipment definition rejected {#rs-eqp-006}

**Structured namespace · Typical level: error**

**Condition:** Equipment definition rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Equipment · **Producer:** `src/Loader/DragonWildsEquipmentLoader.cpp`

**What to check**

Review the identified equipment definition and the original validation error; this entry was rejected.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-EQP-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-eqp-006)

</section>

## Gameplay effects — structured RS-GE {#structured-family-rs-ge}

<section class="rs-code-card" data-rs-code="RS-GE-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-GE-001: Duplicate effect alias {#rs-ge-001}

**Structured namespace · Typical level: error**

**Condition:** Duplicate effect alias. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Gameplay effects · **Producer:** `src/Loader/DragonWildsGameplayEffectLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-GE-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-ge-001)

</section>

<section class="rs-code-card" data-rs-code="RS-GE-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-GE-002: Effect class could not be registered {#rs-ge-002}

**Structured namespace · Typical level: error**

**Condition:** Effect class could not be registered. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Gameplay effects · **Producer:** `src/Loader/DragonWildsGameplayEffectLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-GE-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-ge-002)

</section>

<section class="rs-code-card" data-rs-code="RS-GE-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-GE-003: Effect alias reload deferred {#rs-ge-003}

**Structured namespace · Typical level: warning**

**Condition:** Effect alias reload deferred. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Gameplay effects · **Producer:** `src/Loader/DragonWildsGameplayEffectLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-GE-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-ge-003)

</section>

<section class="rs-code-card" data-rs-code="RS-GE-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-GE-004: Effect definition rejected {#rs-ge-004}

**Structured namespace · Typical level: error**

**Condition:** Effect definition rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Gameplay effects · **Producer:** `src/Loader/DragonWildsGameplayEffectLoader.cpp`

**What to check**

The Gameplay Effect definition was deferred until restart. The record does not mean the effect was applied live.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-GE-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-ge-004)

</section>

## Imports — structured RS-IMP {#structured-family-rs-imp}

<section class="rs-code-card" data-rs-code="RS-IMP-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-001: Import directive rejected {#rs-imp-001}

**Structured namespace · Typical level: error**

**Condition:** Import directive rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Review the rejected $Imp* directives and the exact parser error. No import was executed by the rejected request.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-001)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-002: Import target incompatible {#rs-imp-002}

**Structured namespace · Typical level: error**

**Condition:** Import target incompatible. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Inspect the destination's Modules.Modules array and Module object-reference layout before retrying the import.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-002)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-IMP-003: Import remap ignored {#rs-imp-003}

**Structured namespace · Typical level: warning**

**Condition:** Import remap ignored. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-003)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-004: Import source unresolved {#rs-imp-004}

**Structured namespace · Typical level: error**

**Condition:** Import source unresolved. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-004)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-005: Import source incompatible {#rs-imp-005}

**Structured namespace · Typical level: error**

**Condition:** Import source incompatible. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-005)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-006" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-IMP-006: Optional import dependency skipped {#rs-imp-006}

**Structured namespace · Typical level: warning**

**Condition:** Optional import dependency skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-006)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-007: Module dependency resolution failed {#rs-imp-007}

**Structured namespace · Typical level: error**

**Condition:** Module dependency resolution failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-007)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-008: Atomic import blocked during resolution {#rs-imp-008}

**Structured namespace · Typical level: error**

**Condition:** Atomic import blocked during resolution. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

An atomic import was blocked during dependency resolution. Review the preceding dependency errors; the destination was not changed by this batch.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-008)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-009: Module import plan rejected {#rs-imp-009}

**Structured namespace · Typical level: error**

**Condition:** Module import plan rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-009)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-IMP-010: Atomic import plan blocked {#rs-imp-010}

**Structured namespace · Typical level: error**

**Condition:** Atomic import plan blocked. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-010)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-011" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-IMP-011: Dry-run import plan blocked {#rs-imp-011}

**Structured namespace · Typical level: warning**

**Condition:** Dry-run import plan blocked. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

This was a blocked dry-run plan, not attempted execution. Inspect the dependency and validation errors before running without $ImpDry.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-011)

</section>

<section class="rs-code-card" data-rs-code="RS-IMP-012" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-IMP-012: Import batch partially applied {#rs-imp-012}

**Structured namespace · Typical level: warning**

**Condition:** Import batch partially applied. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Imports · **Producer:** `src/Loader/DragonWildsAssetModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-IMP-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-imp-012)

</section>

## Journal — structured RS-JRN {#structured-family-rs-jrn}

<section class="rs-code-card" data-rs-code="RS-JRN-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-001: Journal runtime classes unavailable {#structured-rs-jrn-001}

**Structured namespace · Typical level: error**

This label also has a [legacy reference meaning](#rs-jrn-001); do not combine the two diagnoses.

**Condition:** Journal runtime classes unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#structured-rs-jrn-001)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-002: Invalid journal document root {#structured-rs-jrn-002}

**Structured namespace · Typical level: error**

This label also has a [legacy reference meaning](#rs-jrn-002); do not combine the two diagnoses.

**Condition:** Invalid journal document root. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

The journal definition root is invalid. Review the expected top-level structure of the source file.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#structured-rs-jrn-002)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-003: Invalid journal entry {#rs-jrn-003}

**Structured namespace · Typical level: error**

**Condition:** Invalid journal entry. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-003)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-004: Journal patch target unavailable {#rs-jrn-004}

**Structured namespace · Typical level: error**

**Condition:** Journal patch target unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-004)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-005: Journal patch rejected {#rs-jrn-005}

**Structured namespace · Typical level: error**

**Condition:** Journal patch rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-005)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-006: Journal ownership conflict {#rs-jrn-006}

**Structured namespace · Typical level: error**

**Condition:** Journal ownership conflict. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-006)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-007: Cooked journal declarations rejected {#rs-jrn-007}

**Structured namespace · Typical level: error**

**Condition:** Cooked journal declarations rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-007)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-008: Journal persistence event unavailable {#rs-jrn-008}

**Structured namespace · Typical level: error**

**Condition:** Journal persistence event unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-008)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-009: Journal persistence event layout incompatible {#rs-jrn-009}

**Structured namespace · Typical level: error**

**Condition:** Journal persistence event layout incompatible. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-009)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-010: Journal unlock callback failed {#rs-jrn-010}

**Structured namespace · Typical level: error**

**Condition:** Journal unlock callback failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-010)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-011: Journal persistence hook unavailable {#rs-jrn-011}

**Structured namespace · Typical level: error**

**Condition:** Journal persistence hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-011)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-012" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-012: Journal event component invalid {#rs-jrn-012}

**Structured namespace · Typical level: error**

**Condition:** Journal event component invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-012)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-013" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JRN-013: Journal unlock unavailable {#rs-jrn-013}

**Structured namespace · Typical level: error**

**Condition:** Journal unlock unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-013)

</section>

<section class="rs-code-card" data-rs-code="RS-JRN-014" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-JRN-014: Journal acquisition observer unavailable {#rs-jrn-014}

**Structured namespace · Typical level: warning**

**Condition:** Journal acquisition observer unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Journal · **Producer:** `src/Loader/DragonWildsJournalModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JRN-014](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-jrn-014)

</section>

## Configuration and definitions — structured RS-JSON {#structured-family-rs-json}

<section class="rs-code-card" data-rs-code="RS-JSON-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JSON-001: JSON definition could not be opened / parsed {#rs-json-001}

**Structured namespace · Typical level: error**

**Condition:** JSON definition could not be opened / parsed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Configuration and definitions · **Producer:** `src/Core/JsonDocument.cpp`

**What to check**

Review the reported source file for JSON/JSONC syntax problems. Use line and column evidence only when the parser actually supplied them.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JSON-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-json-001)

</section>

<section class="rs-code-card" data-rs-code="RS-JSON-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JSON-002: Required JSON field missing {#rs-json-002}

**Structured namespace · Typical level: error**

**Condition:** Required JSON field missing. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Configuration and definitions · **Producer:** `src/Utility/Config.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JSON-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-json-002)

</section>

<section class="rs-code-card" data-rs-code="RS-JSON-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JSON-003: JSON field has wrong type {#rs-json-003}

**Structured namespace · Typical level: error**

**Condition:** JSON field has wrong type. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Configuration and definitions · **Producer:** `src/Utility/Config.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JSON-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-json-003)

</section>

<section class="rs-code-card" data-rs-code="RS-JSON-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JSON-004: JSON safety limit exceeded {#rs-json-004}

**Structured namespace · Typical level: error**

**Condition:** JSON safety limit exceeded. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Configuration and definitions · **Producer:** `src/Core/JsonDocument.cpp`

**What to check**

The JSONC input exceeded the supported document-size limit. Split definitions into smaller independent files.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JSON-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-json-004)

</section>

<section class="rs-code-card" data-rs-code="RS-JSON-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-JSON-005: JSON file-discovery safety limit exceeded {#rs-json-005}

**Structured namespace · Typical level: error**

**Condition:** JSON file-discovery safety limit exceeded. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Configuration and definitions · **Producer:** `src/Utility/Config.cpp`

**What to check**

The section exceeded its supported file-count limit. Reduce the number of files in that section.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-JSON-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-json-005)

</section>

## Loaders — structured RS-LOAD {#structured-family-rs-load}

<section class="rs-code-card" data-rs-code="RS-LOAD-001" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-LOAD-001: Loader folder discovery failed {#rs-load-001}

**Structured namespace · Typical level: warning**

**Condition:** Loader folder discovery failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Loaders · **Producer:** `src/Loader/DragonWildsModLoaderBase.cpp`

**What to check**

Review the affected loader directory and the original discovery failure before editing mod definitions.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-LOAD-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-load-001)

</section>

<section class="rs-code-card" data-rs-code="RS-LOAD-002" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-LOAD-002: Loader capability unavailable {#rs-load-002}

**Structured namespace · Typical level: warning**

**Condition:** Loader capability unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Loaders · **Producer:** `src/Loader/DragonWildsModLoaderBase.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-LOAD-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-load-002)

</section>

<section class="rs-code-card" data-rs-code="RS-LOAD-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-LOAD-003: Loader section raised an exception {#rs-load-003}

**Structured namespace · Typical level: warning**

**Condition:** Loader section raised an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Loaders · **Producer:** `src/Loader/DragonWildsModLoaderBase.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-LOAD-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-load-003)

</section>

<section class="rs-code-card" data-rs-code="RS-LOAD-004" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-LOAD-004: Loader section raised an unknown exception {#rs-load-004}

**Structured namespace · Typical level: warning**

**Condition:** Loader section raised an unknown exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Loaders · **Producer:** `src/Loader/DragonWildsModLoaderBase.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-LOAD-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-load-004)

</section>

## Niagara — structured RS-NIA {#structured-family-rs-nia}

<section class="rs-code-card" data-rs-code="RS-NIA-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NIA-001: Invalid Niagara document root {#rs-nia-001}

**Structured namespace · Typical level: error**

**Condition:** Invalid Niagara document root. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Niagara · **Producer:** `src/Loader/DragonWildsNiagaraLoader.cpp`

**What to check**

The Niagara definition root was rejected. Review the specified source file and expected root structure.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NIA-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-nia-001)

</section>

<section class="rs-code-card" data-rs-code="RS-NIA-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NIA-002: Duplicate Niagara ID {#rs-nia-002}

**Structured namespace · Typical level: error**

**Condition:** Duplicate Niagara ID. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Niagara · **Producer:** `src/Loader/DragonWildsNiagaraLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NIA-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-nia-002)

</section>

<section class="rs-code-card" data-rs-code="RS-NIA-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NIA-003: Niagara definition rejected {#rs-nia-003}

**Structured namespace · Typical level: error**

**Condition:** Niagara definition rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Niagara · **Producer:** `src/Loader/DragonWildsNiagaraLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NIA-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-nia-003)

</section>

<section class="rs-code-card" data-rs-code="RS-NIA-004" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NIA-004: Niagara reload requires restart {#rs-nia-004}

**Structured namespace · Typical level: warning**

**Condition:** Niagara reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Niagara · **Producer:** `src/Loader/DragonWildsNiagaraLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NIA-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-nia-004)

</section>

## NPCs — structured RS-NPC {#structured-family-rs-npc}

<section class="rs-code-card" data-rs-code="RS-NPC-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-001: Merchant interaction hook unavailable {#rs-npc-001}

**Structured namespace · Typical level: error**

**Condition:** Merchant interaction hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-001)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-002" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-002: NPC client world-state presentation ignored {#rs-npc-002}

**Structured namespace · Typical level: warning**

**Condition:** NPC client world-state presentation ignored. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-002)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-003" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-003: NPC client replica preparation skipped {#rs-npc-003}

**Structured namespace · Typical level: warning**

**Condition:** NPC client replica preparation skipped. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-003)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-004: NPC lifecycle construction exhausted {#rs-npc-004}

**Structured namespace · Typical level: error**

**Condition:** NPC lifecycle construction exhausted. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-004)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-005" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-005: NPC construction or attachment blocked {#rs-npc-005}

**Structured namespace · Typical level: error**

**Condition:** NPC construction or attachment blocked. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-005)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-006: NPC cleanup queue failed {#rs-npc-006}

**Structured namespace · Typical level: error**

**Condition:** NPC cleanup queue failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-006)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-007: NPC quarantine failed {#rs-npc-007}

**Structured namespace · Typical level: error**

**Condition:** NPC quarantine failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-007)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-008" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-008: NPC retirement not confirmed {#rs-npc-008}

**Structured namespace · Typical level: error**

**Condition:** NPC retirement not confirmed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-008)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-009: NPC interaction attachment failed {#rs-npc-009}

**Structured namespace · Typical level: error**

**Condition:** NPC interaction attachment failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-009)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-010: NPC interaction could not open {#rs-npc-010}

**Structured namespace · Typical level: error**

**Condition:** NPC interaction could not open. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-010)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-011: Saved quest receipt import failed {#rs-npc-011}

**Structured namespace · Typical level: error**

**Condition:** Saved quest receipt import failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-011)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-012" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-012: Saved dialogue state import failed {#rs-npc-012}

**Structured namespace · Typical level: error**

**Condition:** Saved dialogue state import failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/DragonWildsNpcLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-012)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-013" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-013: Quest acquisition callback encountered an exception {#rs-npc-013}

**Structured namespace · Typical level: error**

**Condition:** Quest acquisition callback encountered an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestAcquisition.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-013)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-014" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-014: Quest acquisition item resolution failed {#rs-npc-014}

**Structured namespace · Typical level: warning**

**Condition:** Quest acquisition item resolution failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestAcquisition.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-014](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-014)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-015" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-015: Quest progress notification failed {#rs-npc-015}

**Structured namespace · Typical level: warning**

**Condition:** Quest progress notification failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestAcquisition.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-015](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-015)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-016" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-016: Quest acquisition credit could not be completed {#rs-npc-016}

**Structured namespace · Typical level: error**

**Condition:** Quest acquisition credit could not be completed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestAcquisition.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-016](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-016)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-017" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-017: Automatic quest hand-in encountered an exception {#rs-npc-017}

**Structured namespace · Typical level: error**

**Condition:** Automatic quest hand-in encountered an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestCompletion.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-017](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-017)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-018" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-018: Quest objective text presentation failed {#rs-npc-018}

**Structured namespace · Typical level: warning**

**Condition:** Quest objective text presentation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestPresentation.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-018](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-018)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-019" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-019: Pending NPC cleanup retry failed {#rs-npc-019}

**Structured namespace · Typical level: error**

**Condition:** Pending NPC cleanup retry failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcCleanup.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-019](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-019)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-020" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-020: Dialogue gate evaluation failed {#rs-npc-020}

**Structured namespace · Typical level: warning**

**Condition:** Dialogue gate evaluation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-020](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-020)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-021" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-NPC-021: Dialogue choice unlock evaluation failed {#rs-npc-021}

**Structured namespace · Typical level: warning**

**Condition:** Dialogue choice unlock evaluation failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-021](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-021)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-022" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-022: Dialogue event action encountered an exception {#rs-npc-022}

**Structured namespace · Typical level: error**

**Condition:** Dialogue event action encountered an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-022](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-022)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-023" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-023: Dialogue quest action encountered an exception {#rs-npc-023}

**Structured namespace · Typical level: error**

**Condition:** Dialogue quest action encountered an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-023](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-023)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-024" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-024: Dialogue shop opening failed {#rs-npc-024}

**Structured namespace · Typical level: error**

**Condition:** Dialogue shop opening failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-024](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-024)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-025" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-025: Dialogue reward operation encountered an exception {#rs-npc-025}

**Structured namespace · Typical level: error**

**Condition:** Dialogue reward operation encountered an exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-025](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-025)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-026" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-026: NPC equipment world state could not be refreshed {#rs-npc-026}

**Structured namespace · Typical level: error**

**Condition:** NPC equipment world state could not be refreshed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcEquipment.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-026](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-026)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-027" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-027: Dialogue reward receipt remains pending {#rs-npc-027}

**Structured namespace · Typical level: error**

**Condition:** Dialogue reward receipt remains pending. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-027](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-027)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-028" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-028: Dialogue shop transition failed {#rs-npc-028}

**Structured namespace · Typical level: error**

**Condition:** Dialogue shop transition failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcDialogue.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-028](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-028)

</section>

<section class="rs-code-card" data-rs-code="RS-NPC-029" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-NPC-029: Quest-scroll activation encountered an unknown exception {#rs-npc-029}

**Structured namespace · Typical level: error**

**Condition:** Quest-scroll activation encountered an unknown exception. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** NPCs · **Producer:** `src/Loader/NpcQuestScroll.inl`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-NPC-029](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-npc-029)

</section>

## Data tables — structured RS-RAW {#structured-family-rs-raw}

<section class="rs-code-card" data-rs-code="RS-RAW-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RAW-001: Raw patch target row not found {#rs-raw-001}

**Structured namespace · Typical level: error**

**Condition:** Raw patch target row not found. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Data tables · **Producer:** `src/Loader/DragonWildsRawTableLoader.cpp`

**What to check**

Check the requested raw DataTable target and confirm that it exists in the current game content.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RAW-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-raw-001)

</section>

<section class="rs-code-card" data-rs-code="RS-RAW-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RAW-002: Raw reload target DataTable unavailable {#rs-raw-002}

**Structured namespace · Typical level: error**

**Condition:** Raw reload target DataTable unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Data tables · **Producer:** `src/Loader/DragonWildsRawTableLoader.cpp`

**What to check**

The resolved raw target was not a usable DataTable object. Confirm the current game class and expected target type.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RAW-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-raw-002)

</section>

<section class="rs-code-card" data-rs-code="RS-RAW-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RAW-003: Registry patch plan rejected {#rs-raw-003}

**Structured namespace · Typical level: error**

**Condition:** Registry patch plan rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Data tables · **Producer:** `src/Loader/DragonWildsRawTableLoader.cpp`

**What to check**

A raw registry plan was rejected. Inspect the planner evidence and the exact requested row or target before retrying.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RAW-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-raw-003)

</section>

## Recipes — structured RS-REC {#structured-family-rs-rec}

<section class="rs-code-card" data-rs-code="RS-REC-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-REC-001: Recipe definition root is invalid {#rs-rec-001}

**Structured namespace · Typical level: error**

**Condition:** Recipe definition root is invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Recipes · **Producer:** `src/Loader/DragonWildsRecipeModLoader.cpp`

**What to check**

The recipe document root was rejected. Check the expected top-level recipe format.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-REC-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rec-001)

</section>

<section class="rs-code-card" data-rs-code="RS-REC-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-REC-002: Recipe identity is reserved {#rs-rec-002}

**Structured namespace · Typical level: error**

**Condition:** Recipe identity is reserved. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Recipes · **Producer:** `src/Loader/DragonWildsRecipeModLoader.cpp`

**What to check**

A recipe entry or its identity/body was invalid. Inspect the named recipe object and required fields.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-REC-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rec-002)

</section>

## Registry — structured RS-RGST {#structured-family-rs-rgst}

<section class="rs-code-card" data-rs-code="RS-RGST-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-001: Registry entry rejected {#rs-rgst-001}

**Structured namespace · Typical level: error**

**Condition:** Registry entry rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-001)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-002: Registry document rejected {#rs-rgst-002}

**Structured namespace · Typical level: error**

**Condition:** Registry document rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-002)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-003: Invalid registry owner {#rs-rgst-003}

**Structured namespace · Typical level: error**

**Condition:** Invalid registry owner. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-003)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-004: Cooked registry asset rejected {#rs-rgst-004}

**Structured namespace · Typical level: error**

**Condition:** Cooked registry asset rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-004)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-005" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-RGST-005: Cooked registry discovery limited {#rs-rgst-005}

**Structured namespace · Typical level: warning**

**Condition:** Cooked registry discovery limited. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-005)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-006: Cooked registry discovery failed {#rs-rgst-006}

**Structured namespace · Typical level: error**

**Condition:** Cooked registry discovery failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-006)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-007" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-RGST-007: Registry changes require a restart {#rs-rgst-007}

**Structured namespace · Typical level: warning**

**Condition:** Registry changes require a restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Loader/DragonWildsRegistryLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-007)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-008" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-RGST-008: Native registry binding unavailable {#rs-rgst-008}

**Structured namespace · Typical level: warning**

**Condition:** Native registry binding unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-008)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-009" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-RGST-009: Native registry exclusion unavailable {#rs-rgst-009}

**Structured namespace · Typical level: warning**

**Condition:** Native registry exclusion unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-009)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-010: Native registrar unavailable {#rs-rgst-010}

**Structured namespace · Typical level: error**

**Condition:** Native registrar unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-010)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-011" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-011: Journal native identity unverified {#rs-rgst-011}

**Structured namespace · Typical level: error**

**Condition:** Journal native identity unverified. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-011)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-012" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-012: Cooked registry lane unavailable {#rs-rgst-012}

**Structured namespace · Typical level: error**

**Condition:** Cooked registry lane unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-012)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-013" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-013: Cooked registry lane rejected {#rs-rgst-013}

**Structured namespace · Typical level: error**

**Condition:** Cooked registry lane rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-013)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-014" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-RGST-014: Cooked asset discovery unavailable {#rs-rgst-014}

**Structured namespace · Typical level: warning**

**Condition:** Cooked asset discovery unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-014](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-014)

</section>

<section class="rs-code-card" data-rs-code="RS-RGST-015" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-RGST-015: Cooked discovery safety cap exceeded {#rs-rgst-015}

**Structured namespace · Typical level: error**

**Condition:** Cooked discovery safety cap exceeded. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Registry · **Producer:** `src/Misc/DragonWildsDataRegistrar.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-RGST-015](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-rgst-015)

</section>

## Spawns — structured RS-SPAWN {#structured-family-rs-spawn}

<section class="rs-code-card" data-rs-code="RS-SPAWN-001" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-001: Spawn document root is invalid {#structured-rs-spawn-001}

**Structured namespace · Typical level: error**

This label also has a [legacy reference meaning](#rs-spawn-001); do not combine the two diagnoses.

**Condition:** Spawn document root is invalid. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

The spawn document root must be an array. Check the named file before retrying.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#structured-rs-spawn-001)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-002" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-002: Spawn definition was rejected {#rs-spawn-002}

**Structured namespace · Typical level: error**

**Condition:** Spawn definition was rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

The spawn entry or patch declaration was rejected. Inspect the named entry and its allowed fields.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-002)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-003: Spawn patch target not loaded {#rs-spawn-003}

**Structured namespace · Typical level: error**

**Condition:** Spawn patch target not loaded. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

The spawn patch identity was not found in the loaded spawn registry. Confirm the identity of the existing spawn record.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-003)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-004" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-004: Spawn registration rejected {#rs-spawn-004}

**Structured namespace · Typical level: error**

**Condition:** Spawn registration rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Spawn registration failed validation or reported an exception. Review the exact evidence and required fields, including Location when applicable.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-004](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-004)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-005" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-SPAWN-005: Spawn authoring requires restart {#rs-spawn-005}

**Structured namespace · Typical level: warning**

**Condition:** Spawn authoring requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

This spawn change requires restart rather than live reload. Do not interpret Deferred as a failed startup registration.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-005](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-005)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-006" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-006: Spawn lifecycle hook unavailable {#rs-spawn-006}

**Structured namespace · Typical level: error**

**Condition:** Spawn lifecycle hook unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-006](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-006)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-007" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-007: Spawn tick unavailable {#rs-spawn-007}

**Structured namespace · Typical level: error**

**Condition:** Spawn tick unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-007](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-007)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-008" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-SPAWN-008: Spawn actor property missing {#rs-spawn-008}

**Structured namespace · Typical level: warning**

**Condition:** Spawn actor property missing. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-008](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-008)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-009" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-009: Spawn actor property write failed {#rs-spawn-009}

**Structured namespace · Typical level: error**

**Condition:** Spawn actor property write failed. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-009](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-009)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-010" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-SPAWN-010: Native AI base class unavailable {#rs-spawn-010}

**Structured namespace · Typical level: error**

**Condition:** Native AI base class unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-010](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-010)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-011" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-SPAWN-011: Activity observer unavailable {#rs-spawn-011}

**Structured namespace · Typical level: warning**

**Condition:** Activity observer unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-011](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-011)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-012" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-SPAWN-012: Activity hook limit reached {#rs-spawn-012}

**Structured namespace · Typical level: warning**

**Condition:** Activity hook limit reached. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-012](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-012)

</section>

<section class="rs-code-card" data-rs-code="RS-SPAWN-013" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-SPAWN-013: Native activity event unavailable {#rs-spawn-013}

**Structured namespace · Typical level: warning**

**Condition:** Native activity event unavailable. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Spawns · **Producer:** `src/Loader/DragonWildsSpawnLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-SPAWN-013](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-spawn-013)

</section>

## Strings — structured RS-STR {#structured-family-rs-str}

<section class="rs-code-card" data-rs-code="RS-STR-001" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-STR-001: String override conflict {#rs-str-001}

**Structured namespace · Typical level: warning**

**Condition:** String override conflict. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Strings · **Producer:** `src/Loader/DragonWildsStringModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-STR-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-str-001)

</section>

<section class="rs-code-card" data-rs-code="RS-STR-002" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-STR-002: String replacements unmatched {#rs-str-002}

**Structured namespace · Typical level: warning**

**Condition:** String replacements unmatched. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Strings · **Producer:** `src/Loader/DragonWildsStringModLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-STR-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-str-002)

</section>

<section class="rs-code-card" data-rs-code="RS-STR-003" data-rs-namespace="structured" data-rs-severity="error" markdown="1">

### RS-STR-003: String definition rejected {#rs-str-003}

**Structured namespace · Typical level: error**

**Condition:** String definition rejected. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Strings · **Producer:** `src/Loader/DragonWildsStringModLoader.cpp`

**What to check**

The string override document was rejected. Check the file structure before making string-level changes.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-STR-003](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-str-003)

</section>

## Vendors — structured RS-VND {#structured-family-rs-vnd}

<section class="rs-code-card" data-rs-code="RS-VND-001" data-rs-namespace="structured" data-rs-severity="warning" markdown="1">

### RS-VND-001: Vendor reload requires restart {#rs-vnd-001}

**Structured namespace · Typical level: warning**

**Condition:** Vendor reload requires restart. Read the actual message and context for the precise rejected, deferred or partial operation.

**Area:** Vendors · **Producer:** `src/Loader/DragonWildsVendorLoader.cpp`

**What to check**

Start with the original message and the listed mod, file, target or property. The first related failure and reported phase/outcome help narrow down the next step. If a reading is unavailable, collect the surrounding messages before changing paths or disabling a mod.

Keep the exact source, mod, file, target/property, phase and outcome when supplied. Missing context or an unknown cause is not a zero reading or a confirmed diagnosis. Review personal identifiers and paths before sharing evidence.

[Direct link to structured RS-VND-001](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-vnd-001)

</section>

## Bridge reason codes

These lower-level values appear as `reasonCode` in `NetworkBridgeDiagnostics.json`. Read them together with `stage`, `outcome`, `channel` and `correlationId`; accepted stages are not necessarily completed actions. Some exception classifications are coarse hints. Receipts can also carry bounded free-text detail: arbitrary detail is not a new registered code. Known reasons include a `referenceUrl` pointing here; unknown detail is preserved without inventing a diagnosis.

<section class="rs-code-card" data-rs-code="accepted" markdown="1">

### accepted: Accepted at this stage {#bridge-accepted}

**Why it occurs**

The envelope or validation step passed, not necessarily the whole gameplay action.

**What to check**

Follow the same correlationId through terminal outcome and readback before claiming delivery or execution.

[Direct link to accepted](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-accepted)

</section>

<section class="rs-code-card" data-rs-code="native_authority_bypass" markdown="1">

### native_authority_bypass: Already on native authority {#bridge-native-authority-bypass}

**Why it occurs**

The observed action is already executing on the authoritative side and needs no client forwarding.

**What to check**

Normally no action is required. Do not forward it again.

[Direct link to native_authority_bypass](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-native-authority-bypass)

</section>

<section class="rs-code-card" data-rs-code="bridge_not_ready" markdown="1">

### bridge_not_ready: Bridge not ready {#bridge-bridge-not-ready}

**Why it occurs**

The player bridge is unavailable, not attached or still waiting to replicate; retries may remain pending or expire.

**What to check**

Wait for world/player readiness. If it persists, check cooked bridge availability and the first attachment error on host and client.

[Direct link to bridge_not_ready](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-bridge-not-ready)

</section>

<section class="rs-code-card" data-rs-code="duplicate_observation" markdown="1">

### duplicate_observation: Duplicate observation suppressed {#bridge-duplicate-observation}

**Why it occurs**

The same local action already has a pending request.

**What to check**

Normally no action is required; suppression prevents duplicate execution. Inspect the original correlation rather than submitting another action.

[Direct link to duplicate_observation](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-duplicate-observation)

</section>

<section class="rs-code-card" data-rs-code="inventory_confirmed" markdown="1">

### inventory_confirmed: Native inventory removal observed {#bridge-inventory-confirmed}

**Why it occurs**

The server observed the exact consumed-item removal needed to create a short-lived authority permit.

**What to check**

Follow permit matching and callback completion; this intermediate observation is not proof of the final gameplay effect.

[Direct link to inventory_confirmed](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-inventory-confirmed)

</section>

<section class="rs-code-card" data-rs-code="executed" markdown="1">

### executed: Execution reported at this stage {#bridge-executed}

**Why it occurs**

The callback, activation or receipt reports execution.

**What to check**

Use the original stage and any final native readback. A receipt is not interchangeable with verification of every downstream effect.

[Direct link to executed](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-executed)

</section>

<section class="rs-code-card" data-rs-code="callback_failed" markdown="1">

### callback_failed: Callback failed {#bridge-callback-failed}

**Why it occurs**

An authority callback threw or a request handler sent its failure receipt.

**What to check**

Read the first associated exception and correlationId. Check the registered graph, data and native layout before retrying; do not repeat a potentially consumed item blindly.

[Direct link to callback_failed](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-callback-failed)

</section>

<section class="rs-code-card" data-rs-code="inventory_not_confirmed" markdown="1">

### inventory_not_confirmed: Consumption not confirmed {#bridge-inventory-not-confirmed}

**Why it occurs**

The authority has no matching unexpired native inventory-removal permit, or its wait expired.

**What to check**

Check the consumable instance and native consumption path on the host. Do not invoke the effect directly or grant a duplicate item as a workaround.

[Direct link to inventory_not_confirmed](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-inventory-not-confirmed)

</section>

<section class="rs-code-card" data-rs-code="player_disconnected" markdown="1">

### player_disconnected: Player no longer available {#bridge-player-disconnected}

**Why it occurs**

A pending request lost its original player object.

**What to check**

Reconnect and explicitly select the current character. Old requests must not be redirected to a new player object.

[Direct link to player_disconnected](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-player-disconnected)

</section>

<section class="rs-code-card" data-rs-code="internal_error" markdown="1">

### internal_error: Unclassified bridge failure {#bridge-internal-error}

**Why it occurs**

An exception did not match the bridge's coarse diagnostic classification.

**What to check**

Capture the original exception, channel, stage and correlationId. This fallback alone does not identify a specific root cause.

[Direct link to internal_error](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-internal-error)

</section>

<section class="rs-code-card" data-rs-code="legacy_transport" markdown="1">

### legacy_transport: Legacy transport used {#bridge-legacy-transport}

**Why it occurs**

The request was transmitted using the supported legacy route.

**What to check**

This is not automatically an error. Follow its receipt and keep client/server framework versions compatible.

[Direct link to legacy_transport](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-legacy-transport)

</section>

<section class="rs-code-card" data-rs-code="rate_limited" markdown="1">

### rate_limited: Request rate exceeded {#bridge-rate-limited}

**Why it occurs**

The bridge's request budget for this requester was exceeded.

**What to check**

Stop repeated clicks or automated resubmission and retry at a normal rate. Mod authors should deduplicate requests rather than disable the limit.

[Direct link to rate_limited](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-rate-limited)

</section>

<section class="rs-code-card" data-rs-code="stale_revision" markdown="1">

### stale_revision: Stale request revision {#bridge-stale-revision}

**Why it occurs**

The incoming revision is not newer than the requester's last accepted revision.

**What to check**

Refresh the current state and send a genuinely new request only if still needed. Never replay old envelopes.

[Direct link to stale_revision](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-stale-revision)

</section>

<section class="rs-code-card" data-rs-code="deferred" markdown="1">

### deferred: Authority work deferred {#bridge-deferred}

**Why it occurs**

A request was accepted but awaits its required authority-side condition.

**What to check**

Follow the same correlation to completion or expiry. Accepted/deferred is not completed; do not trigger a second grant.

[Direct link to deferred](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-deferred)

</section>

<section class="rs-code-card" data-rs-code="incompatible" markdown="1">

### incompatible: Compatibility handshake rejected {#bridge-incompatible}

**Why it occurs**

The compatibility receipt was unsuccessful. A protocol conflict, different shared registry definitions, or different ordered native network-ID lanes rejects compatibility. Platform mapping fingerprints are informational.

**What to check**

Read the handshake detail and compare enabled mod versions, load order and native network-ID fingerprints. Align conflicts, restart and reconnect. Older peers without owner/lane telemetry are unverified; no IDs are automatically rewritten.

[Direct link to incompatible](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-incompatible)

</section>

<section class="rs-code-card" data-rs-code="registry_missing" markdown="1">

### registry_missing: Registry action missing {#bridge-registry-missing}

**Why it occurs**

The bridge classified an exception containing 'not registered'. The required action key may not have enrolled.

**What to check**

Inspect the original exception and owning declaration. Resolve enrollment before retrying; this classification is guidance, not an independent registry check.

[Direct link to registry_missing](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-registry-missing)

</section>

<section class="rs-code-card" data-rs-code="callback_contract_mismatch" markdown="1">

### callback_contract_mismatch: Callback contract mismatch {#bridge-callback-contract-mismatch}

**Why it occurs**

The bridge classified an exception mentioning a layout or contract mismatch.

**What to check**

Inspect the exact reflected function/parameters and supported game build. Use compatible components, never guessed offsets.

[Direct link to callback_contract_mismatch](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-callback-contract-mismatch)

</section>

<section class="rs-code-card" data-rs-code="expired" markdown="1">

### expired: Request expired {#bridge-expired}

**Why it occurs**

A diagnostic result reports expiry after its allowed wait.

**What to check**

Inspect its stage and original detail to identify the missing prerequisite. Refresh current state before making a new request.

[Direct link to expired](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-expired)

</section>

<section class="rs-code-card" data-rs-code="world_changed" markdown="1">

### world_changed: World changed {#bridge-world-changed}

**Why it occurs**

Pending work was cancelled at the old world's boundary.

**What to check**

Normally no repair is required. Wait for the new world and explicitly select current targets before starting new actions.

[Direct link to world_changed](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#bridge-world-changed)

</section>

</div>

## Coverage and external codes

The audit scans first-party runtime and Helpy source, plugins and build scripts for literal RS codes and typed diagnostic IDs. Test-only future-code fixtures and generated catalogues are excluded. It rejects undocumented codes and unknown typed IDs. Every registered code and bridge reason must have causes, troubleshooting and a unique generated header. The documentation build also validates the rendered HTML anchors.

Windows error numbers, HRESULTs, HTTP status codes, UE4SS messages and EOS results belong to their respective providers; they are not RuneSchema RS codes. Keep their original value and surrounding message. Ordinary log tags such as `[PERSISTENCE-PRUNER]` identify a subsystem, not an error number.

`RS-GAME-*` entries are RuneSchema troubleshooting references for native game messages, not codes emitted by the game. Helpy marks symptom matches as suggested, preserves the original message and severity, and uses `RS-GAME-000` when no specific native diagnosis matches.

## For mod authors

Emit a registered code at the actual decision site. Never reuse it for another condition, classify absent readings as zero, or turn performance attribution into network failure. Keep dynamic paths, counts and native details in the original message.

The legacy catalogue lives at `resources/diagnostics/codes.json`, its checks at `resources/diagnostics/guidance.json`, and bridge reasons at `resources/diagnostics/bridge-reasons.json`. The structured catalogue is read from `source/raw/include/Core/StructuredDiagnosticRegistry.h`, with available condition-specific checks from Helpy's `StructuredDiagnosticAdvice.h`; it is not merged into the legacy typed-ID enum. Run `python build/generate-diagnostics.py --check` and `python build/audit-diagnostics.py`; runtime and Helpy headers, public JSON and this page must agree. Existing RS anchors are retained; shared-label structured meanings have separate namespace anchors. Never derive diagnoses from arbitrary text.

See [troubleshooting](TROUBLESHOOTING.md).
