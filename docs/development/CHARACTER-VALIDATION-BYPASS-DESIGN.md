# Character validation bypass design

Status: paired native return bypass implemented in the alpha candidate.
An installed RuneSchema-only corrupted-character load remains to be tested.

RuneSchema lets native character validation and player-state loading finish,
then accepts their rejected Boolean return results. This matches the two native
functions intercepted by the original CorruptCharacterBypass. The bypass itself
does not fabricate character data, replace native loading, or reset progress.

## Exact interception boundary

The intended sequence is:

1. The game reads and processes the character through its native loading path.
2. Each intercepted native function runs with its original arguments.
3. At its verified return epilogue, after native processing and result production
   finish, RuneSchema inspects the Boolean result.
4. A valid result remains unchanged. An invalid result becomes valid when the
   bypass was enabled at startup.
5. The original register and stack restoration completes, and the function
   returns to its original caller.

The x64 implementation uses two register-context mid-hooks: one at the native
character validator's return tail and one at `ProcessPlayerStateLoad`'s return
tail. Only a zero `AL` becomes `1`; the upper bits of `RAX` remain intact. The
hooks do not jump past native processing, manually call a native function through
an assumed C++ prototype, or catch and suppress crashes. Their scope covers all
callers of these two functions, not just one character-load call site.

The validator can perform work needed to populate character state. Returning
success from its entry point without executing it is therefore outside this
design. A forced success result cannot undo changes made inside either function.

## Activation and discovery

Retain `persistence.allowCorruptCharacterEntry`, off by default. A restart applies
both enabling and disabling. Do not add a competing setting or alter UE4SS.dll.

Before activation, identify both native functions using unique executable-code
matches at exact x64 unwind entries. Steam and Game Pass have separate verified
player-state entry shapes; variable displacements and stack sizes are masked.
Decode each complete function and require one normal return, preceded by at
least five bytes of register and stack restoration that do not overwrite `AL`.
Reject unknown tail exits, indirect branches, malformed boundaries, multiple
returns, and branches that could enter the epilogue after the hook.

Do not depend on fixed executable addresses or a version label alone. If an
update changes the instruction shape, add a separately verified variant rather
than patching the first approximate match. An absent or ambiguous contract leaves
native acceptance unchanged and reports the specific discovery failure.

Refuse duplicate hooks when the standalone `CorruptCharacterBypass` is enabled
or loaded. Its `enabled.txt` marker overrides an attempted `mods.txt : 0`
disable. Never silently disable another mod. Prepare both hooks disabled,
publish their ownership, and enable them only after every check passes. If either
preparation or activation fails, remove both; do not leave a partial bypass.

## Save and progress boundaries

The bypass itself must not write a character save, remove inventory, clear
quests, reset skills, grant unlocks, substitute a default character, or edit Xbox
Game Save containers. Existing registry cleanup is separate and must not become
more permissive because the bypass is enabled. Unfamiliar IDs are not proof of
orphaned content.

A successful bypass means only that this rejection gate was overridden. It does
not prove that the character fully deserialized or retained all progress. The
game's subsequent saves can persist incomplete state. Recovery testing therefore
uses a verified backup and disposable copy, not the only live character. General
save repair and automatic save suppression are not part of this change.

## Diagnostics

Keep normal logging concise: report readiness once, the first actual rejection
override once per native function, or an actionable activation failure. Distinguish
missing validator, unverified paired return gates, standalone conflict, hook
preparation failure and hook activation failure. Do not report READY merely because the
setting is enabled, or report a character recovered merely because a hook fired.

Advanced diagnostics may include the storefront, executable fingerprint,
discovery stage, candidate counts, selected relative addresses, original versus
effective acceptance and a bypass counter. Do not log save contents or character
identifiers. Capture these diagnostics without adding file I/O or repeated scans
to the acceptance callback.

## Verification required before installation

- With the switch off, native acceptance and rejection remain unchanged.
- With the switch on, native validation runs exactly once with unchanged register
  and stack arguments. A false result becomes true at each verified return tail;
  true results remain unchanged. Both functions' callers receive these results.
- Callback removal restores native rejection. Hook activation failures and
  ambiguous or malformed discovery inputs install no bypass.
- Run the same discovery and hook lifecycle checks against Steam and Game Pass.
  Synthetic Boolean tests alone do not establish installed-game compatibility.
- In an isolated profile with the original bypass completely disabled, reproduce
  a rejection with the RuneSchema switch off, then load the same disposable
  character with it on. Confirm an actual BYPASSED event, not just READY.
- Compare character identity, skills and XP, quest state, recipes, journals,
  inventory and equipment before and after the test. Separate unavoidable native
  loading changes from unexplained loss. Test normal save, exit and reload before
  claiming progress preservation.

## Verification and remaining game test

`source/raw/src/Core/CharacterEntryRecovery.cpp` owns the paired hooks.
`CharacterEntryBypassPlan::FindReturn` checks decoded return-flow coverage, and
`CharacterEntryReturnGate.h` supplies the production x64 decoder.
`CharacterEntryBypassPlan::AcceptRejected` changes only a rejected low byte.

The production decoder accepts private code fixtures for both native functions
on Steam and Game Pass. The isolated hook fixture verifies native execution
once per call, six preserved register and stack arguments, rejected-result
acceptance, unchanged successful results, disabled preparation, and restoration
of native rejection after removal. These checks do not establish a successful
in-game corrupted-character load.

The user reported a rejected character with the previous single-gate bypass. The October 16
Game Pass session instead logged a standalone-bypass conflict and loaded the
original mod. That run does not identify the earlier failure or validate
RuneSchema alone. Preserve separate outcomes for activation, an observed
rejection override, character loading, and progress retention when investigating
the failure. The paired candidate replaces that narrower implementation; test it
with the standalone mod completely disabled before claiming in-game success.

For backup and recovery procedures, see [Manual save recovery](MANUAL-SAVE-RECOVERY.md).

## Selective pruning investigation — 19 October 2026

The target is a composed load path, not a bypass that rewrites characters:
prepare world-dependent definitions, prune proven invalid records, allow native
hydration to run, observe whether acceptance bypass was needed, then verify
world entry and a subsequent persisted save independently. Pending declarations
must not veto general cleanup; their exact typed identities are retained while
other certified orphans are removed.

The installed Steam executable contains distinct diagnostics for missing item
data, failure to create an item, and failure to load item state. Food, drink and
potion diagnostics also include explicit "skipping" variants. These static
strings support investigating selective native rejection, but do not establish
that all loaders skip only one record, or provide a verified callable callback.
The cached October 16 Steam disassembly also shows early failure branches in
ProcessPlayerStateLoad before later loading stages. Accepting its return cannot
retroactively execute those skipped stages. This finding is scoped to that
audited function version, not a new binding for every current executable.

Two failure classes must remain separate:

- A persistence identity absent from a certified applicable registry can make
  all records depending on that absent asset orphaned.
- A valid asset whose instance state failed to deserialize is a record-level
  failure. Repair must identify its container/slot and verify the exact original
  record before removing that instance. Other instances sharing the same ID
  must remain intact. A failure Boolean alone cannot authorize this removal.

Future record-level repair needs a verified native failure producer with attempt
identity, record location, failure classification and record-content evidence.
It must preserve the original before applying a bounded edit and must not retry
native hydration via an assumed ABI or loop repeatedly after failure. Unfamiliar
fields are not automatically unreadable data. No such native rejection callback
or automatic native retry was added in this investigation.

Logging must reflect evidence rather than an assumed success chain:

1. GAME-REJECTED-RECORD only after native record-level evidence; otherwise name
   the cause REGISTRY-CONFIRMED-ORPHAN, never claim the game rejected it.
2. PRUNED-PAYLOAD after a verified in-memory transformation; include record kind
   and count. This is not a disk-save receipt.
3. BYPASSED only when a verified acceptance hook changes a rejected result.
4. WORLD-ENTERED only after an actual world/player readiness observation.
5. SAVE-VERIFIED only after native persistence and readback prove the specific
   removals and preservation of unaffected data. Normal WGS/provider rules still
   apply; do not bypass the provider and edit its files.

Current registry cleanup and acceptance bypass are implemented separately.
Native instance-rejection capture, attempt-correlated world-entry/persistence
receipts and a genuine failing-save round-trip remain unverified. Do not print
the proposed complete success story before those events are independently seen.

## Native tolerant loading and the developer proposal — 19 October 2026

The user supplied a screenshot of a game developer proposing a character-save
"modded" flag: relax loading when removed mods leave unexpected data, preserve
readable progress, and return to strict validation after a clean unmodded load.
The message explicitly describes an idea, not a shipped field or API. Do not add
an invented flag to native character JSON, clear unknown native metadata, or
claim this proposal already applies to world saves.

Read-only analysis of the installed Steam executable found the existing
`dom.PersistenceStrictMode` console variable. Its native help describes missing
or unknown player-save data as a load failure under strict mode. The missing
ItemData branch reads this variable; when tolerant, that branch can return a
present optional containing no item rather than the failure result. This is
evidence for selective missing-item tolerance in that audited executable, not
proof that every malformed instance, oversized inventory or world record can be
recovered. Instance-state deserialization failure is a separate native branch.

The local implementation incorporates the proposal's loading principle without
inventing a native save-format field:

1. Keep registry-confirmed orphan pruning independent of acceptance bypass.
   Exact pending declarations protect only their own typed IDs.
2. On clients, `quickLaunch.mode` chooses Vanilla Load, Less Restrictive or
   Quick Launch. The latter two hold native strict mode off across loads and
   menu returns for the session. Save Bypass is independent in every client mode.
   Dedicated servers have only the bypass loading toggle: it also requests the
   session tolerant policy, without selecting or starting a world on their behalf.
3. Resolve and validate reflected console getter/setter layouts. Read the
   original value, set strict mode off only when it was on, and require readback.
   Unavailable/unknown values never count as successful activation. A failed
   setter/readback attempts rollback.
4. Verify policy at map and character-load boundaries, and reapply it if native
   code reset the variable. Do not restore strictness when a load returns or
   Quick Launch finishes/cancels. Restore the original value only at shutdown
   when possible. An already-tolerant original value is never changed to strict.
   Failed restoration is explicitly logged as unverified.
5. Correlate committed payload-prune counts and actual acceptance overrides with
   a thread-local load token. Nested ambiguous loads suppress further exact
   attribution; unrelated threads cannot inherit that token.

The console variable is process-wide and remains off for the selected session.
This is not a native per-character modded flag and does not promise recovery of
asynchronous/direct native loader failures. The mode is saved in RuneSchema's
settings.ini, but no native game configuration is written. An unverified
restoration must remain an explicit
failure, not a promise that the live value was restored. No original save is
edited by this policy lease, and world-save recovery remains a separate path.
Live validation must establish
whether these supported callback boundaries cover the relevant native hydration
and whether console writes are available on each storefront.

`PRUNED-PAYLOAD` is emitted only after native argument writeback succeeds.
`HANDLER-RETURNED` reports the scoped counts but explicitly does not claim world
entry or persisted removal. Native record-rejection capture, world-entry
correlation, and a native save/readback receipt remain future work. The proposed
modded flag must not be cleared merely because a handler returned or a Boolean
bypass fired. If a real native flag is introduced later, use its verified schema
and lifecycle rather than guessing its spelling or semantics.
