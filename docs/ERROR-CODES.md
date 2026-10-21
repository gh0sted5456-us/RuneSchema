# Diagnostic code reference

RuneSchema's stable diagnostic catalogue. A code identifies a condition, not a crash severity: the emitted message and severity remain authoritative. **Suggested references** for older uncoded messages are labelled as suggestions, not assigned causes.

The Live Console shows a readable event title, timestamp, repeated-event count and details with meaning, impact and next steps. Raw entries remain copyable; the viewer never changes disk logs.

This page contains **49 stable RS codes** and **19 bridge reason codes**. Every entry has its own permanent header link on this page. A catalogue entry is not a claim that every operation already emits that code: older messages can remain uncoded and suggested references stay explicitly labelled.

## Read a code safely

1. Click the code in Helpy's Live Console, or select the event and choose **Open code reference** (also available when the window is narrow). The link opens this page at that code's header.
2. Read the original message, level and first preceding failure. The typical level below is not a replacement for the event's actual severity.
3. Follow the condition-specific checks below; keep last-good caches and original saves. Report the exact code, subsystem, game/storefront/framework versions and a redacted excerpt if it persists.

!!! warning "Before sharing logs"
    Review player names, character IDs and local paths. Opening a reference page sends no log content.

<div class="rs-code-tools"><label for="rs-code-search">Find a code or symptom</label><input id="rs-code-search" type="search" placeholder="RS-CACHE-003, bridge_not_ready, chest, journal..." autocomplete="off"><p id="rs-code-count" aria-live="polite"></p></div>

## Code families

| Family | Area |
| --- | --- |
| RS-QST | Quests |
| RS-REG | Registry |
| RS-NET | Multiplayer |
| RS-WEAR | Equipment |
| RS-PERF | Performance |
| RS-CAT | Items |
| RS-SPAWN | Spawns |
| RS-LOG | Console |
| RS-RT | Runtime |
| RS-CACHE | Cache |
| RS-DL | Downloads |
| RS-JRN | Journal |
| RS-SAVE | Save safety |
| RS-EQUIP | Equipment |
| RS-PARTY | Party |
| RS-MAP | Map |
| RS-CHEST | Storage |
| RS-ITEM | Items |
| RS-SRC | Content |
| RS-GAME | Game items |

<div class="rs-code-catalogue" markdown="1">

<section class="rs-code-card" data-rs-code="RS-QST-001" data-rs-severity="error" markdown="1">

## RS-QST-001: Conflicting quest ownership {#rs-qst-001}

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

<section class="rs-code-card" data-rs-code="RS-REG-001" data-rs-severity="warning" markdown="1">

## RS-REG-001: Registry enrollment is not ready {#rs-reg-001}

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

<section class="rs-code-card" data-rs-code="RS-NET-001" data-rs-severity="error" markdown="1">

## RS-NET-001: Client and server content differ {#rs-net-001}

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

<section class="rs-code-card" data-rs-code="RS-WEAR-001" data-rs-severity="warning" markdown="1">

## RS-WEAR-001: Wearable contract is not enrolled {#rs-wear-001}

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

<section class="rs-code-card" data-rs-code="RS-PERF-001" data-rs-severity="warning" markdown="1">

## RS-PERF-001: A runtime task took too long {#rs-perf-001}

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

<section class="rs-code-card" data-rs-code="RS-CAT-001" data-rs-severity="warning" markdown="1">

## RS-CAT-001: Item catalogue is not ready {#rs-cat-001}

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

<section class="rs-code-card" data-rs-code="RS-SPAWN-001" data-rs-severity="warning" markdown="1">

## RS-SPAWN-001: Placement needs loaded ground {#rs-spawn-001}

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

<section class="rs-code-card" data-rs-code="RS-LOG-000" data-rs-severity="info" markdown="1">

## RS-LOG-000: Unclassified diagnostic {#rs-log-000}

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

## RS-LOG-001: File logging is unavailable {#rs-log-001}

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

## RS-LOG-002: Export destination is protected {#rs-log-002}

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

<section class="rs-code-card" data-rs-code="RS-RT-001" data-rs-severity="error" markdown="1">

## RS-RT-001: Native API contract changed {#rs-rt-001}

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

## RS-RT-002: Runtime is becoming ready {#rs-rt-002}

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

<section class="rs-code-card" data-rs-code="RS-CACHE-001" data-rs-severity="info" markdown="1">

## RS-CACHE-001: Mounted content changed {#rs-cache-001}

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

## RS-CACHE-002: Cache bundle format is invalid {#rs-cache-002}

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

## RS-CACHE-003: Cache does not match installed content {#rs-cache-003}

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

## RS-CACHE-004: Cache integrity check failed {#rs-cache-004}

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

## RS-CACHE-005: Cache storage limit reached {#rs-cache-005}

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

## RS-CACHE-006: Cache operation cancelled {#rs-cache-006}

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

## RS-CACHE-007: Bundle has partial coverage {#rs-cache-007}

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

## RS-CACHE-008: Content owner cannot be verified {#rs-cache-008}

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

## RS-CACHE-009: Cache update could not be committed {#rs-cache-009}

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

## RS-CACHE-010: Legacy icon pack loaded {#rs-cache-010}

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

<section class="rs-code-card" data-rs-code="RS-DL-001" data-rs-severity="warning" markdown="1">

## RS-DL-001: Download is unavailable {#rs-dl-001}

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

## RS-DL-002: Download manifest was rejected {#rs-dl-002}

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

<section class="rs-code-card" data-rs-code="RS-JRN-001" data-rs-severity="info" markdown="1">

## RS-JRN-001: Journal cache is an observed subset {#rs-jrn-001}

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

## RS-JRN-002: Journal cache could not be read {#rs-jrn-002}

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

<section class="rs-code-card" data-rs-code="RS-SAVE-001" data-rs-severity="warning" markdown="1">

## RS-SAVE-001: Original save is protected {#rs-save-001}

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

## RS-SAVE-002: Persistence cleanup is not certified {#rs-save-002}

**Save safety · Typical level: warning**

Complete stable registry evidence is unavailable.

**Common reasons**

Complete stable item/recipe/registry evidence is unavailable, so removal cannot be distinguished safely from incomplete loading.

**What RuneSchema does**

Automatic pruning stays disabled; no character save is modified by this deferral.

**What to do next**

Resolve readiness first; a missing lookup is not proof of removed content.

**Troubleshooting checks**

Resolve earlier enrollment failures and wait for stable registration. This deferral does not modify a character save. Never treat an unresolved lookup as permission to prune content.

[Direct link to RS-SAVE-002](https://gh0sted5456-us.github.io/RuneSchema/ERROR-CODES/#rs-save-002)

</section>

<section class="rs-code-card" data-rs-code="RS-EQUIP-001" data-rs-severity="warning" markdown="1">

## RS-EQUIP-001: Auto-equip is unsupported or ambiguous {#rs-equip-001}

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

## RS-EQUIP-002: Equip result is not confirmed {#rs-equip-002}

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

<section class="rs-code-card" data-rs-code="RS-PARTY-001" data-rs-severity="info" markdown="1">

## RS-PARTY-001: Player reading is unavailable {#rs-party-001}

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

## RS-PARTY-002: Host authority is required {#rs-party-002}

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

## RS-PARTY-003: Player or world identity changed {#rs-party-003}

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

<section class="rs-code-card" data-rs-code="RS-MAP-001" data-rs-severity="warning" markdown="1">

## RS-MAP-001: Map target is not loaded {#rs-map-001}

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

## RS-MAP-002: Resource respawn time is unverified {#rs-map-002}

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

<section class="rs-code-card" data-rs-code="RS-CHEST-001" data-rs-severity="warning" markdown="1">

## RS-CHEST-001: Chest access contract is unverified {#rs-chest-001}

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

## RS-CHEST-002: Storage inventory is ambiguous {#rs-chest-002}

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

<section class="rs-code-card" data-rs-code="RS-ITEM-001" data-rs-severity="warning" markdown="1">

## RS-ITEM-001: Some items were not delivered {#rs-item-001}

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

<section class="rs-code-card" data-rs-code="RS-SRC-001" data-rs-severity="warning" markdown="1">

## RS-SRC-001: Content source is not mounted {#rs-src-001}

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

<section class="rs-code-card" data-rs-code="RS-GAME-001" data-rs-severity="error" markdown="1">

## RS-GAME-001: Item power-level row name is invalid {#rs-game-001}

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

## RS-GAME-002: Cooked animation skeleton GUID is stale {#rs-game-002}

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

## RS-GAME-003: Native package or object was not found {#rs-game-003}

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

## RS-GAME-004: Material shader data is invalid {#rs-game-004}

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

## RS-GAME-005: Blueprint accessed a missing object {#rs-game-005}

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

<section class="rs-code-card" data-rs-code="RS-GAME-000" data-rs-severity="warning" markdown="1">

## RS-GAME-000: Native game warning or error {#rs-game-000}

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

<section class="rs-code-card" data-rs-code="RS-GAME-006" data-rs-severity="warning" markdown="1">

## RS-GAME-006: Vitals UI callback cleanup failed {#rs-game-006}

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

## RS-GAME-007: Native user settings unavailable {#rs-game-007}

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

## RS-GAME-008: Native crowd navigation unavailable {#rs-game-008}

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

## RS-GAME-009: Native tagged-object cleanup mismatch {#rs-game-009}

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

The compatibility receipt was unsuccessful. The current session handler rejects a different protocol; other differences can be reported permissively.

**What to check**

Read the original handshake detail and compare protocol/framework versions. Do not assume every registry or mapping difference rejects the session.

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

The stable catalogue lives at `resources/diagnostics/codes.json`, condition-specific checks at `resources/diagnostics/guidance.json`, and bridge reasons at `resources/diagnostics/bridge-reasons.json`. Run `python build/generate-diagnostics.py --check` and `python build/audit-diagnostics.py`; runtime and Helpy headers, public JSON and this page must agree. Use `PS::Diagnostics::Url(entry)` for RS links and `BridgeReasonUrl(reason)` for known bridge reasons; never maintain a second reference URL or derive diagnoses from arbitrary text.

See [troubleshooting](TROUBLESHOOTING.md).
