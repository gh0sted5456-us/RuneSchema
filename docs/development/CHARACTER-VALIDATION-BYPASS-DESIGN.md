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
