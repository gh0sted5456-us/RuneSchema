# Quest-scroll Item identity correction — 2026-10-15

Candidate: `RuneSchema-0.7.8.0-alpha-quest-scroll-identity-r1-20261015.zip`.
Built/tested and installed on Steam/Game Pass at the user's request (2026-10-15),
with backups and hash verification. Includes the existing r3 quests/NPC-equipment
work and shared `RuneSchema.QuestItems` content. Helpy and stock UE4SS are unchanged.

## Reported failure and evidence boundary

Game Pass r3 reported `Quest-scroll item identity is not canonical` for all
tested board contracts. Its two identity reads shared that message, so the old
log cannot establish whether the asset PersistenceID or Item instance cache was
the failing field. Provider audit found all nine bound IDs canonical and all
installed provider files matching their sealed candidates. No item or quest ID
is regenerated, and no binding/content change is required by this correction.

An isolated Steam menu-only native probe reproduced a valid state that r3
rejects. A disposable `/Script/Dominion.Item` initialized through native
`SetItemData` had all of the following:

- Loaded vanilla ItemData PersistenceID `1k8-cj3JqEGeRgu1P7PK2g`, with FString
  storage Num=23, Max=24, trailing NUL. A normal FString copy retained Num=23.
- Native `ItemData` pointer matching that asset, and `BP_GetItemData` returning
  the same exact asset.
- `ItemDataPersistenceId` still empty (Num=0, Max=0) before **and after** native
  initialization and the getter. Flags were `0x0048000001002200`: SaveGame and
  Transient, without Net/RepNotify. This is not a mandatory replicated live ID.

This establishes an overstrict runtime check, not a string-copy defect. The
probe used a private fixture, a transient Item and private configuration/save
directories; no player inventory, world or existing save was opened or changed.
It is not a live crafted-item or saved/reloaded Game Pass acceptance test.

Evidence: workspace `.cache/quest-scroll-identity-evidence-20261015/native-steam-item-lifecycle.log`,
SHA-256 `57CE6C73FAD33335DC612A31075C28DA0C8C076DFD2AA35E9CB5768D0EF0BE8E`.
Probe source/build/start scripts are workspace `.cache/quest-scroll-identity-native-probe.cpp`,
`build-quest-scroll-identity-native-probe.ps1` and `start-quest-scroll-identity-native-probe.ps1`.
Only this task's exact private probe processes were stopped after testing.

## Runtime policy

The asset's ID remains mandatory, canonical, nonempty and equal to the existing
binding. Reads borrow reflected FString storage; header, capacity, bounded 22
ASCII characters, terminator and canonical final bits are checked before a
copy. Missing fields, changed layouts, corrupt headers, malformed values and
populated-but-mismatched cache IDs are rejected before acceptance or payment.

An empty instance cache is allowed only after the existing owned/current/loaded
main-inventory and exact slot checks, live ItemData pointer match, and a new
native `Item:BP_GetItemData` result matching the same asset. The cache field must
also retain its reflected nonreplicated SaveGame/Transient semantics. The cache
is never repaired, filled, cleared or rewritten. Client forwarding still sends
only the normal owned inventory/slot RPC, not a QuestID, asset or reward.

The UseItem override, original-body suppression, provenance check, positive slot
count, controller/pawn/world guards, authority execution, operation guard,
acceptance receipt, start-cost handling and payout logic remain in place.
Duplicate-active, cooldown, missing-prerequisite and uncertain transactions do
not get a new consumption/reward path.

Rejections now identify `[QUEST-SCROLL][IDENTITY]`, field name, object path, class,
expected ID, storage Num/Max, bounded characters and reason. Malformed text is
not unboundedly read or printed as arbitrary log content. A subsequent Game Pass
failure can distinguish the asset from the instance cache without guessing.

## Verification and remaining acceptance

- 183/183 core tests pass with normal temporary-fixture access, including new
  bounded-string/cache-policy tests and strengthened routing checks. The initial
  sandbox run's 11 filesystem-fixture failures passed outside the sandbox.
- 75 generated-schema checks pass; no schema or existing IDs change.
- Native build passes, both DLLs are UPX-compressed/integrity-tested, and all
  269 core imports match both installed stock Steam/Game Pass UE4SS hosts.
- Extracted compressed Helpy passes private hidden-window/server-desktop
  lifecycle, manifest/capability, INI, standalone-import, branding and license checks.

Still required in the installed build: new crafted
and saved/reloaded scrolls, both `JobBoard_Jobs` and Silver for the Restless,
exactly one scroll consumed on acceptance, duplicate-active and cooldown rejection
without consumption, and authoritative multiplayer routing. If the actual Game
Pass warning is caused by another field/state, preserve the new diagnostic and
investigate it; the old generic warning alone cannot prove the affected field.

Verified ZIP SHA-256: `5279EE59F0D5CC44552C428EBEE2425542AEFA858A121DA07AF5D5FDEAA3BBF6`.
Compressed core: `4EB60FF501495F8EFB5AC8E5F8BA04E13B002C2DED70AFE8296B32DB5B84F654`.
Helpy: `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.
