# Editor and persistence follow-up — 19 October 2026

These changes are local follow-up work, not a replacement of the published
0.8.0.1 release. Installed Steam/Game Pass copies and original saves are not
modified by the build or synthetic tests.

## Character editor and shared presentation

- Searchable character selection, human-readable metadata names where available,
  and platform marks. Shared native JSON shows Steam and Epic together; an
  imported JSON does not acquire Xbox provenance from the running game.
- Switching away from an open draft requires confirmation. Selecting the same
  save does not discard its draft. Offline confirmation remains required.
- Preview requests start automatically after opening a character. The native
  renderer can use a local-controller world as well as the front-end world.
  Actors remain transient, non-replicated and collision-free. Native pose/asset
  preparation is asynchronous: first-frame latency is not zero.
- Economy (256 px/10 fps), Balanced (384 px/20 fps), and Detail (512 px/30 fps)
  capture presets; a fixed bounded 512 px transport remains compatible. Idle
  frames are reused. Animation and technical preview details are opt-in.
- Appearance controls remain available with the preview hidden. Whole editor
  panes no longer dim/undim with each background authoring request. Mutating
  operations still pass through the existing single-operation admission gate.
- Convert/export has a three-step presentation. JSON export is supported; a
  native Xbox WGS conversion adapter is **not** implemented or simulated.
- Item-delivery selection shows player names, not party vitals. Review controls
  are centered; advanced/raw workflows are hidden unless enabled.
- Social marks are padding-trimmed and contain-fitted to the same square canvas.
  Nexus's orange layer follows the theme while its white artwork stays white.
- NuLL, Lonestar, iGhrazi and Hauler journal portraits from the Dye project are
  included in the optional GitHub artwork manifest, not runtime release ZIPs.
  Scale comparison supports these choices, a player portrait, and an outline.
- Scale comparison uses measured native single-mesh bounds or collision height
  when available, with an explicit measurement basis and a nominal 1.80 m
  reference. Unknown or multipart geometry is labelled illustration-only.
  Existing unambiguous journal-name matching supplies enemy artwork.
- Successfully imported managed hash-named download packs are removed from
  Helpy's staging directory. User imports, installed mod packs, artwork and
  parsed caches are retained; failed imports are retained for retry.
- Authoring is a route-backed tab under Tools, not a duplicate sidebar/Home
  destination. Old authoring-only preferences still expose the Tools container.

## Confirmed persistence issues repaired

1. Clone validation required a project-specific RS7/mod-prefix/digit shape even
   though the native game accepts canonical 22-character URL-safe base64 IDs.
   Valid existing IDs now use the native format irrespective of their prefix.
2. Clone has visible Generate ID, edit and copy controls. Generation encodes 16
   random bytes; the final base64 character is canonical. Invalid non-empty
   input is reported rather than silently replaced. Native collision checks
   remain mandatory. Existing released IDs are never regenerated automatically.
3. URL-safe '-' and '_' characters are escaped injectively in new live clone
   asset names, not in the persistence identity itself. Legacy alphanumeric
   clone names are unchanged.
4. A successful late native item/quest/journal registration now adds its exact
   identity to the immutable cleanup snapshot after native map verification.
   This cannot certify an incomplete registry. Failure withdraws cleanup
   authority, not a successfully committed native asset.
5. Character-load cleanup is no longer consumed once for the entire client
   process. Each incoming gameplay payload is checked without rescanning or
   mutating native registries on world transitions.
6. Confirmed orphan quests do not require valid ownership markers or QuestInts
   merely to be removed. Valid registered IDs with legacy/conflicting markers
   are retained by automatic orphan-only cleanup. Proven orphan location links
   are removed; unrelated location records remain untouched.
7. Automatic cleanup preserves unfamiliar individual records instead of letting
   one malformed optional identity prevent removal of other confirmed orphans.
   Unsupported container layouts still refuse cleanup rather than guessing.
8. A replacement native FString is verified before committing it. A later
   diagnostic failure no longer falsely reports that a committed payload was
   unchanged. Startup file cleanup retains its existing backup-first checks.

## Verification scope and remaining checks

The final combined synthetic editor suite passed all 64 tests (128.83 seconds),
including navigation, item authoring, compact/minimum layouts and rendered
workspaces. Both native DLL targets also built successfully. The separate
save-cleanup and owned-save-cleanup-contract regressions passed: disposable JSON
fixtures cover registered clone retention, removed-mod inventory/storage/
loadout/quest links, malformed orphan markers, idempotence, and incomplete
registry refusal. The optional-art packaging test passed with 32 managed images
excluded while preserving runtime DLLs, scripts, settings and ICO resources.
These are local validation results, not live-game or published-package approval.

Still required: live local-world preview isolation/appearance checks, measured
resource coverage across real assets, and the reported failure logs/clone
definitions. These repairs do not establish the cause of every external report
or guarantee repair of every unsupported third-party save field. WGS files are
never edited directly; supported provider-backed gameplay JSON is filtered only
at the reflected native load boundary.

## World-entry follow-up and party layout

Power Level is centered above the profile icon in the party card. Its former
right-side space is returned to the name/role row. The rendered fixture checks
that the badge and portrait align horizontally and do not overlap; the full
64-test suite passed after this layout change (134.74 seconds).

At the user's direction, pending declarations do **not** create a global cleanup
barrier. A typed, exact-identity pending set protects only the declared item,
recipe, quest or journal ID while it is waiting for native registration. Other
confirmed orphans are still pruned, including orphans of the same type. The
set is not an ownership ledger, a prefix exemption or proof of successful native
registration. Successful native registration updates the verified snapshot and
releases that declaration's pending entry. An absent mod contributes no pending
entries on the next launch, so its old identities can be removed normally.

The character-load preflight now invokes targeted quest preparation before
scrubbing the reflected JSON. This reuses the existing idempotent quest service;
it does not re-run RegisterAll or sweep assets on world transitions. If that
preparation fails, exact pending IDs remain protected and general cleanup still
runs against certified registries. Existing safeguards for genuinely unavailable
or unhealthy registry authority remain in force. Unsupported save fields are not
treated as orphans merely because they resemble IDs.

Regressions confirm cleanup during pending item/quest/journal/recipe work,
registry-specific protection, valid late registration, repeat-character cleanup,
idempotence, and removal after the mod is absent next launch. The final revised
code passed all 61 non-render Helpy tests; the three renderer tests passed for
the unchanged party UI before the per-ID cleanup revision. Both DLL targets built.
No original saves were modified, and these changes are not installed or published.

Clone format verification was strengthened separately: every generated ID is
decoded to exactly 16 bytes and independently re-encoded, with canonical padding
and all-one/sequence vectors. A 2,048-ID sample also passes native validation; a
22-position by 128-character mutation test confirms UI/native validator agreement.
The generator uses 22 unpadded Base64URL characters with canonical trailing bits;
mod prefixes are not a validity requirement. Existing IDs are not regenerated.

The existing allowCorruptCharacterEntry setting remains opt-in. Its verified
paired return hooks let native loading run and accept rejected return results;
that bypass does not repair a save. No new force-entry or guessed native call was
added. Live world-entry ordering, timings and results still need an in-game check.

## Vortex packaging investigation

The Dragonwilds Vortex extension author documents automatic UE4SS installation,
UE4SS DLL deployment, RuneSchema JSON/JSONC support, and a Game Pass RuneSchema
destination fix: https://www.nexusmods.com/site/mods/1273 . This supports using
the existing extension rather than requiring users to select a destination.

A dedicated framework download should be checked against the extension's actual
installer before being labelled Vortex-ready. In particular, verify that its
RuneSchema framework root and nested Helpy plugin DLL are not flattened or
misclassified as independent UE4SS mods. Keep framework, RuneSchema content
mods, and plugin-only updates distinguishable. Installation, enable/disable,
upgrade and uninstall must be tested separately for Win64 and WinGDK paths;
user settings and generated content must not be packaged as release defaults.

No Vortex extension was installed or modified, no deployment settings changed,
and no new Vortex-compatible release archive was claimed or published during
this investigation.

## Native tolerant-load follow-up and developer suggestion

The supplied developer-chat screenshot proposes an eventual native modded-save
flag, not a confirmed existing field. Its preserve-readable-data principle is
incorporated locally using a load-scoped, opt-in lease of the existing
`dom.PersistenceStrictMode` variable. The installed Steam missing-ItemData branch
was inspected read-only and can tolerate an unresolved item when strict mode is
off. This does not prove recovery of malformed instance state, out-of-bounds
containers, or arbitrary world data.

The reflected getter/setter layouts are checked before use, all value changes
require readback, and the matching post-load callback restores the original
strict value. An unavailable API or failed restoration is not logged as success.
No native save-format flag is invented. Pending typed IDs still protect only
themselves; general registry-confirmed cleanup is not blocked.

Committed payload-prune counts and actual acceptance overrides now share a
thread-local load token. Nested ambiguous loads suppress precise attribution;
`HANDLER-RETURNED` explicitly leaves world entry and disk persistence unverified.
The exact native record-failure producer and a save/readback receipt remain to
be implemented and validated. See
[Character validation bypass design](CHARACTER-VALIDATION-BYPASS-DESIGN.md).

Both native DLL targets built successfully after these changes. All nine
persistence regressions passed, including the acceptance-bypass safety contract,
strict-mode rollback/readback checks, and load-token isolation. The complete
Helpy suite passed all 64 tests, including the three rendered layouts (130.08
seconds). `git diff --check` passed. No original save, installed runtime, public
tag, release archive or GitHub branch was changed by this follow-up. An isolated
native load/save/reload test is still required before claiming retained progress.
