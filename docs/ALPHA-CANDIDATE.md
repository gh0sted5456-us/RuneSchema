# RuneSchema 0.7.8.0-alpha candidate — 2026-10-16

This source snapshot is for `runeschema-7.8-alpha`. It does not change the main or
experimental release channels or create a release tag.

## Grounding, first-building placement and icon census — installed Steam/Game Pass

This follow-up keeps the installed quest-scroll identity correction and adds:

- Intended-floor traces now use both endpoints of a local window: 45 cm above
  and 300 cm below the reference. An absent intended surface remains a streamed
  collision wait; the loader no longer accepts Maxxfilth's observed surface
  67.6 metres below its authored reference. Bare `$` still discovers terrain
  over the full search column first, then selects a local supporting floor.
- NPC actor identities and native validity are checked after deferred spawn and
  preview-class loading. Diagnostics distinguish lifetime/world failure from
  a null, non-component or abstract class. Only lifetime failures can retry,
  at most three construction attempts per map and after owned-proxy retirement
  acknowledgement. The surface is retraced on retry. No arbitrary GC respawn.
- Event AI is constructed at its final scale. Grounded waves preserve their
  authored foot offset, recheck the scaled footprint and reconcile native
  construction movement through collision-checked `K2_TeleportTo`. Final actor
  position, capsule and native validity must still match. Missing collision
  after construction is a whole-wave failure, not a half-wave retry. Explicit
  numeric-Z spawns retain their exact-position guard. Authority, save exclusion,
  power and loot checks are unchanged; no encounter content was changed.
- Helpy no longer requires a pre-existing native world copy of a building.
  It validates concrete cooked building defaults/root and the deferred data
  binding, uses native spawn collision rejection and checks the live binding.
  Temporary placement remains transient/save-excluded; permanent placement
  still reports saved-but-inactive separately from confirmed placement. This is
  admin/authoring placement, not the player's material-cost/snapping build mode.
- An unavailable mounted registry is not committed as a completed item census.
  Equipment assets without an `ITEM_` name are now candidates in both paged
  discovery and Full Scan; all candidates still require native validation on use.
- Bulk icon builds also inspect registered mounted item/building data assets,
  so unloaded mod items can supply icons outside `/UI/` and `/Icons/` folders.
  Source filtering, fingerprint checks, bounded game-thread work and cache-first
  rendering remain in force. Discovery failures are counted and displayed;
  partial discovery cannot report a clean completion.

Core regression tests (184/184), Helpy tests (35/35), generated-schema checks
(75/75), native builds, UPX integrity, both stock-host ABI checks (269 imports)
and extracted Helpy hidden-window/server-desktop lifecycle checks pass. An
isolated native Steam menu probe confirms the Iron Chest's cooked class default
has the expected scene root; it did not construct an actor or open a world.
Installed on Steam and Game Pass at the user's request on 2026-10-16, with
verified backups. Steam: 38 framework files / 283 protected files; Game Pass:
38 / 290. All installed framework hashes and Game Pass launch-alias files match.
No server install, game launch, save edit, content-mod edit, UE4SS.dll replacement
or GitHub push was performed. Real-world Maxxfilth placement, Silver Requiem, first Iron Chest,
streamed-cell recovery and menu/full icon builds still require live acceptance.

Per-fuel processing speed is **not implemented** in this candidate. Native
speed/UI functions were identified, but current-burning-fuel identity,
save/reload restoration and non-stacking composition with native buffs still
need proof. Do not author or advertise a per-fuel schema from the proposed
contract yet; accepted-fuel membership remains owned by the cooked mod content.

Verified local archive: `RuneSchema-0.7.8.0-alpha-grounding-building-cache-r1-20261016.zip`.
Archive SHA-256: `B9A48118A5090D88F3AF72A9A502022B5CF53DE0D6B22DA8B4911189792E07C5`.
Compressed core: `940D53F19C5E3941E62F576E7F7EF1B3365D68BBECC907B110AE38A302D07582`.
Compressed Helpy: `9FDF8CB0A2A9CE4EFB0BA00BCF186BB4B7BFFA89B8D9765B28628F311FA7E6D5`.
Both DLLs use `UPX -9 --best --lzma` and pass integrity tests. They replace the
previous identity-correction binaries while retaining that fix. Stock UE4SS on
both clients remains `D936FD2D66AE9287F0B32AEC5F698AA81FF20EF744EAC43BF62D8475469393DF`.
Install backups: `steam-full-alpha-20261015-378fa5cc01da4c349ed24dce0c147511`
and `gamepass-full-alpha-20261015-0c65b8954aef4d69a45f277fcddf464f` under
workspace `.cache/install-backups`. The legacy date prefix belongs to the helper;
the recorded installation date is 2026-10-16. Local verification logs and the
candidate receipt are under workspace `.cache`.

## Quest-scroll identity correction — installed Steam/Game Pass

`RuneSchema-0.7.8.0-alpha-quest-scroll-identity-r1-20261015.zip` fixes a reproduced
overstrict check on native Item instances: native SetItemData and BP_GetItemData
can agree on the correct asset while the nonreplicated save-identity cache is
still empty. It preserves published IDs, rejects malformed/mismatched populated
identities and retains existing owned-slot/authority/receipt protections.
Detailed failure diagnostics now distinguish the asset from the instance cache.

183/183 core tests, 75 schema checks, UPX integrity, 269 imports against both
stock hosts and isolated extracted Helpy lifecycle checks pass. The native
menu-only microtest is not a live crafted/reloaded Game Pass or multiplayer test.
Installed on Steam and Game Pass at the user's request on 2026-10-15. Both core
DLLs match the verified compressed candidate; Game Pass launch alias checked.
No real inventory/save edit, game shutdown, server update or GitHub push was done.
Mods, settings, enable choices and runtime data were preserved.

Install receipts under `.cache/install-backups/`:

- Steam: `steam-full-alpha-20261015-bdbcd1a2b3fe417a96aa214155bcacda`
  (38 installed files, 283 protected files verified unchanged).
- Game Pass: `gamepass-full-alpha-20261015-d0031498de534277b518067f28d1c8dc`
  (39 installed files, 291 protected files verified unchanged).

- Archive SHA-256: `5279EE59F0D5CC44552C428EBEE2425542AEFA858A121DA07AF5D5FDEAA3BBF6`.
- Core SHA-256: `4EB60FF501495F8EFB5AC8E5F8BA04E13B002C2DED70AFE8296B32DB5B84F654`.
- Helpy unchanged: `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.

See [evidence, policy and live acceptance checklist](QUEST-SCROLL-IDENTITY-DIAGNOSTIC.md).

## Alternative objectives and world-persistent NPC equipment — installed Steam/Game Pass

`RuneSchema-0.7.8.0-alpha-quest-alternatives-world-outfits-r3-20261015.zip`
adds reusable stage `Completion: "Any"` and Human NPC `EquipmentWhen` with
explicit `WorldEverCompleted` policy. Installed on Steam and Game Pass at the
user's request on 2026-10-15, superseding the r6 framework below. Both installs
hash-match the verified compressed core and Helpy. Stock UE4SS and server were
not changed; no GitHub push or game launch was performed by this task.

Install receipts under `.cache/install-backups/`:

- Steam: `steam-full-alpha-20261015-4668a04129e24bccafbca4b3cba5bfb7`
  (38 installed files, 275 protected files verified unchanged).
- Game Pass: `gamepass-full-alpha-20261015-f6db03b6e71141a2b874c6d5ac24d655`
  (38 installed files, 286 protected files verified unchanged, launch alias checked).
- An earlier Game Pass attempt stopped when a client/probe appeared. Its
  backup `gamepass-full-alpha-20261015-9f24ed1b101041548bc420c6e2ab9f7c`
  retains the prior core. After the user closed the game, a fresh guarded
  installation completed and preserved the settings regenerated by that launch.

- One kill-or-hand-in quest identity/run/reward; only the winning hand-in
  consumes items. Completed kill counters win over uncommitted inventory.
- Native counter saves and existing authority/repeat/receipt recovery remain
  in use. Core tests cover the combined rat/meat exchange, repeat, reload,
  both-ready precedence, no duplicate payout and insufficient inventory.
- Equipment latches use the reflected native world-save GUID and an atomic
  RuneSchema sidecar under `runtime/live/saved/progress/worlds`. Native
  completion-world proof prevents another world's completed character quest
  from latching this world. Preserve the sidecar with world backups.
- Matching authored rule IDs replicate through the existing NPC identity;
  no arbitrary client equipment paths or new reward route. Existing previews
  update their wearable loadout without replacing the NPC/interaction/pose.
- 182/182 core tests and 75 positive/negative generated-schema checks pass.
  Initial sandbox-only filesystem failures passed with normal temporary-file
  access. Native build passed; both DLLs are UPX-compressed and integrity-tested.
  269 core imports match each stock Steam/Game Pass UE4SS host. The extracted
  compressed Helpy passed isolated hidden-window/server-desktop lifecycle,
  manifest/capability, INI, license/branding and standalone-import checks.
- Verified r3 archive SHA-256:
  `28557D80589BE61A01B01F972949AFAB49735272FC70073D3B3D34C94F6A2111`.
  Core: `A9A6CA0E47B307B4E049D6625019B01887FC7AE07D34BC3500CE134795BD61FA`.
  Helpy remains `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.
  Earlier r1 packaging was rejected because the sandbox blocked UPX; r2 is
  superseded by r3's completed NPC/quest compatibility guard. Use r3 only.
- COLORS received the contracts and owns actual quest/NPC content. It confirmed
  native rat family classes and the author's 5-Silver repeatable reward; neither
  is hardcoded in the reusable runtime. This framework install does not install
  COLORS' Maxxfilth drafts or independently owned Quest Board content. Mods,
  settings, enable choices and runtime data are preserved; saves are untouched.
- No live quest hand-in, helmet rendering, reconnect, late-join, cell reload
  or dedicated restart has been tested for these new capabilities.

See [contract and acceptance checklist](QUEST-ALTERNATIVES-AND-NPC-EQUIPMENT.md).

## Craftable quest scrolls with included shared content — installed Steam/Game Pass

Combined candidate:
`RuneSchema-0.7.8.0-alpha-quest-scroll-r6-20261015.zip`.
Supersedes the runtime-only r4 below and includes the current menu-icon-cache
work. Installed on Steam and Game Pass at the user's request on 2026-10-15.
Server untouched; no GitHub push. Replaced files were backed up, installed
files hash-verified, and user mods/settings/enable choices preserved. Existing
plugin order files are unchanged; the new manifest is enabled by default.

Install receipts under `.cache/install-backups/`:

- Steam: `steam-full-alpha-20261015-49da64ecc6174c3aa44820c52e084a21`
  (38 installed files, 259 protected files verified unchanged).
- Game Pass: `gamepass-full-alpha-20261015-e7fe730415114c9a931531e9e5103e14`
  (39 installed files, 269 protected files verified unchanged).

Both stock UE4SS hashes remain unchanged. The Game Pass WindowsApps launch
alias resolves to the verified DLLs and shared content triplet. Games were not
launched; live acceptance remains pending as described below.

- Includes enabled content-only `RuneSchema.QuestItems`, with a neutral
  `BP_QuestScroll` and an inert stable-ID ItemData template. No quest IDs,
  rewards, JobBoard/ColorsOfMoney dependency, additional native DLL or modified
  UE4SS is embedded. Mods can cook items using the shared class or `$Clone` the
  template through schema and reference the result from station recipes.
- The native-signature UseItem override is const, scripted and explicitly
  **nonpure**, with a false fallback body. Core binding validation rejects pure
  expressions and native-only interface declarations. Normal inherited CanUse
  behavior remains a live acceptance item to verify.
- Final helper v3 containers decode 2/2 packages on both Steam/Game Pass
  mappings. The editor fallback returned false without an inventory. Packaging
  requires the complete triplet and exact verified hashes, and ships a portable
  content receipt and MIT license. Editable assets/reproduction source/evidence
  are retained separately under `source/content/RuneSchemaQuestItems`.
- Runtime checks owned/loaded/removal-enabled main inventory and exact slot/item
  identity, forwards only the native owned inventory/slot RPC for clients, and
  uses the existing acceptance/repeat/completion receipts. One scroll plus any
  normal start cost is charged only after confirmed native acceptance. Ordinary
  rejection preserves the scroll; uncertain partial writes do not auto-replay
  charges, refunds or rewards.

Verification: **180/180 core tests**, **69 generated-schema checks**, both-store
native container decoding, canonical UPX tests for both DLLs, extracted hidden/
server-desktop Helpy lifecycle and **269 imports** against both unchanged stock
UE4SS hosts. Full world consumption, inherited CanUse, save/reload, controller/
hotbar and dedicated/co-op acceptance remain **not in-game validated**. See
[quest scrolls](QUEST-SCROLLS.md). The private menu-only reflection probe was
stopped after capture and did not enter a world or access existing saves.

- ZIP SHA256: `FBC4DF62490D70EEA86318B087D50D6656A4324FBEB710E1F37EAB23F9B55D34`.
- Compressed core SHA256: `B9DBE1A3261E644BC8E734F925F7F2D8A9A471F757CDA9AC4DF5D05743642492`.
- Compressed Helpy SHA256: `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.

## Earlier runtime-only quest-scroll prototype — superseded by r6

Runtime-only candidate:
`RuneSchema-0.7.8.0-alpha-quest-scroll-r4-20261015.zip`.
Incorporates the menu-icon-cache candidate below. Not installed or pushed.

- Item-side `/assets` `$QuestScroll` binds a cooked/permanent-cloned consumable
  to an existing RuneSchema QuestID, expected canonical item PersistenceID and
  optional explicit quest EntryID. The quest and item remain separate stable
  identities; normal recipes can produce the item. No quest clone or recipe
  unlock is used as a substitute for quest acceptance.
- Pre-consumption UseItem interception requires a real non-native Blueprint
  override with the native semantic signature, not interface-inclusive lookup
  alone. It replaces the cooked use body. Owned loaded main-inventory slot and
  item identity are checked, and the authority uses its own binding. Clients
  forward only the standard native inventory/slot RPC, not an arbitrary QuestID,
  callback or reward.
- Existing acceptance/repeat/reward receipts govern the transition. One scroll
  plus any authored start cost is preflighted and charged after confirmed native
  acceptance. Ordinary rejection does not spend it. Reentry/active/pending state
  does not replay charges; uncertain partial writes remain pending, without an
  invented automatic refund or reward.
- User approved a reusable **included RuneSchema.QuestItems content plugin**.
  The separate content author has cooked the neutral Blueprint and benign
  cloneable template; container path validation/integration remains in progress.
  The runtime-only ZIP above does **not** contain that helper. Native-only
  consumable prototypes are not supported/advertised as working scrolls.

Verification: **179/179 core tests**, **69 schema validation checks**, canonical
UPX checks, extracted hidden-window/server-desktop lifecycle, and **269 imports**
against both installed stock UE4SS hosts. A separate isolated menu-only probe
recorded current consumable/interface/RPC/slot reflection without invoking those
functions, entering a world or accessing existing saves. It was stopped after
capture; no other game/probe process was stopped. Actual scroll consumption,
save/reload and dedicated/co-op acceptance remain pending. See
[quest-scroll contract](QUEST-SCROLLS.md).

- Runtime ZIP SHA256: `BA1B0B2C8F362012F117A5332E6E58E5883FAC4839BF39FF55F36B7C784FE87F`.
- Compressed core SHA256: `94F7BD4812981C3BB0DA1F07461DC1D47236B04B533CDF08AADCE1D30B0F4371`.
- Compressed Helpy SHA256: `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.

## Main-menu source icon-cache builds — packaged, NOT installed

Candidate: `RuneSchema-0.7.8.0-alpha-menu-icon-cache-r3-20261015.zip`.
Incorporates the role-badges/cache-progress candidate below. No installation,
game launch, save edit, server policy change or GitHub push was performed.

- Explicit registered-icon cache build from the existing menu/world renderer,
  without opening a gameplay world or requiring a character save. Discovery
  covers loaded registered DataAsset icon references and mounted UI/icon texture
  metadata, not all art textures or arbitrary unloaded custom Blueprint logic.
- Shared Recipes station/source-root classifier separates core, plugin/feature
  roots and managed/schema mod sources. Exact clone creator metadata is retained
  when known; virtual paths/declarations are not claimed as physical per-asset
  container provenance.
- Source-filtered capture and `.rsmk-icons` export, including retained source
  labels through import and re-export. Suggested mod-specific attachment names,
  compatible-disk reuse, bounded capture, cancellation and honest stage/counters.
- Native plugin/GameFeature container/descriptor changes invalidate source
  comparisons. Work stops if source fingerprints change mid-capture. No full
  automatic startup render sweep or stock UE4SS changes.

Verification: **177/177 core tests**, **35/35 Helpy tests**, three viewport
previews, canonical UPX validation, extracted hidden-window/server-desktop
lifecycle, and all **269 imports** against both installed stock UE4SS hosts.
Read-only filesystem source scans completed for both installed game trees;
stock host hashes remain unchanged. Live main-menu bulk capture and installed
content acceptance remain pending. The normal spawn/icon tick service must be
enabled; a headless renderer cannot capture icons. See [icon cache](HELPY-ICON-CACHE.md).

- ZIP SHA256: `41737E5C90551CAAAC789FDEE6A9442F0C6696F54F897BA1F4C26E1310FC7FDB`.
- Compressed core SHA256: `C6DEBF904FD1E0F7D45E9D0BEC40A1C2D656EBEB157915AEB355E40023B96B86`.
- Compressed Helpy SHA256: `E93C1EC074379A4BBD7EFDA1DECE9D1EEAFF1831BA81BF5A69112F93BCD2287E`.

## Live role badges, cache progress and retained Bay details — verified, NOT installed

Candidate: `RuneSchema-0.7.8.0-alpha-player-badges-cache-progress-r4-20261015.zip`.
This incorporates the clone-icon/multi-recipient candidate below and supersedes
the local r1/r2/r3 badge prototypes. No installation, server policy edit, stock
UE4SS change or GitHub push was performed.

- Read-only native owner/admin/host/ordinary-privilege identifiers beside Helpy
  roster names and map players. The authoritative host supplies live status for
  dedicated/co-op play; revisions, owning bridge identity and world-generation
  fences reject stale remote replies. Character GUIDs join map presentation,
  not account admin authentication. Five-second coalesced reads never occupy
  the inventory/spawn/travel mutation slot; stale badges expire after 12 seconds.
- Settings provides **Dump / export cached icons**, import and full catalogue
  scan controls. Cached thumbnails load before native capture; a missing icon
  falls back to capture/save. Dumping exports compatible captured icons only,
  not every game texture. No full sweep runs automatically at launch.
- Active staged cache-operation progress reports content-verification bytes,
  icon counts, writing and final publication, with cancellation. Full scan has
  its own real counters/unknown-total animation and independent cancellation;
  it is not cancelled through the gameplay command's cancellation flag.
- Docked/detached Item Bay preserves hover stats/descriptions, context actions
  and a collapsible selected-item details pane, with reload/copy-path controls.
  The same native inspection cache and selection survive docking. Tooltip
  bounds follow the actual main/Bay surface rather than the combined canvas.

Verification: **177/177 native core tests**, **32/32 standalone UI tests**,
including native-flag interpretation, mask-14 non-admin behavior, expiry,
identity/replay fences, role snapshot transport, non-blocking status hints,
cancelled-export preservation and the production detached stats-pane renderer.
Three viewport previews pass. Canonical UPX tests and extracted hidden-window/
server-desktop lifecycle tests pass; all 269 core imports match both installed
stock UE4SS hosts, whose hashes remain unchanged.

Live dedicated/co-op badge/replication, full installed-content scan/capture and
in-game icon acceptance remain pending. Native account-owner lookup, Admin
grant/revoke/kick controls and target-aware remote mutation receipts are still
separate outstanding work. A role icon is never a Helpy authorization token.

- ZIP SHA256: `A77B55BBBA696E87DF4D29E007FE34B313C6411A5944411981651A33EB7123EA`.
- Compressed core SHA256: `97DDA9608520EBB4B7B6A1F1753A033C48ADD1289085289D34970BFA1DADB7C8`.
- Compressed Helpy SHA256: `51808D7EF2BECBC28F7BDFAC30215E5C761E37FB575D127C8167D56A7AB06616`.

## Clone icon continuity and same-cart recipients — verified, NOT installed

Candidate: `RuneSchema-0.7.8.0-alpha-clone-icons-multiplayer-r2-20261015.zip`.
This supersedes the local r1 build of this pass. It is not installed; Game Pass
remains on the installed schema-parity/icon-packs r4 below, plus the explicitly
requested two-file MaxCape schema test. No push or permission changes performed.

- The normal tracing-off icon resolver now reaches native item-to-icon source
  tracking. Previously its early return skipped that step, affecting icons
  referenced from another content mount, including schema clones.
- Exact native inspection/creation icon confirmations update the browsing bay,
  list/cards, cart metadata, drag payloads and cached query rows. Empty native
  icons remain empty rather than reviving stale inherited art. Draft previews
  remain separate from receipts for the unchanged source item. The inspected
  Steel Full Helm Clone definition already has its custom Scorned Wilderness
  icon; no rewrite of that user definition was made.
- A cart can select up to 16 authoritative recipients for the same quantities.
  Requests serialize per recipient; an item clears only after every recipient's
  exact inventory confirmation. Partial/ambiguous results stop and require
  inventory review. Old self-targeted remote GUID batches are rejected before
  submission, not looped against the caller. See
  [cart recipients and authority boundaries](HELPY-CART-RECIPIENTS.md).

Verification: **177/177 native core tests**, **30/30 standalone UI tests**,
including correlated two-recipient transport, partial-failure/no-retry behavior,
cross-source icon regression checks and native icon metadata into the cart.
Canonical UPX compression tests and extracted desktop lifecycle checks pass;
all 269 imports match both installed stock UE4SS hosts. Live clone-icon and
two-player acceptance remain pending. This does not claim an Admin implementation.

- ZIP SHA256: `DCF443BC241E072BBAE0DF818B1975E1042E5DFA9666A40F5EBE5B7EA074A8DE`.
- Compressed core SHA256: `6CC3A9BEB6955FA39AA010E18B2D1326D30EDB581E8716AA700EADB9F893827D`.
- Compressed Helpy SHA256: `56C4D644E1EBBD77C6F9F116E5600F1D595649F8F1C7842E425BBF0C8D1A7E31`.

### MaxCape contract test result

Two new schema files were installed into Game Pass's existing MaxCape after
backing up and hash-verifying all 10 original mod files. Backup:
`.cache/install-backups/maxcape-wearable-contract-20261015-63465fd28c1046bcb544648d030411d2`.
Original files, DLLs and saves were not changed by that installation. The next
live log resolves both mesh-data dependencies and enumerates the MaxCape item,
but NOT-ENROLLED recurs at 19:39:32/19:39:53 local time. The user reports the cape
is visible with the wrong appearance. This is **not a fixed visual enrollment**.
Its Lua package loads and finds the item; its 40 CPU-second startup delay may
affect timing but is not established as the visual fault's root cause. Native
hide-cache confirmation and Lua texture/material application need further
runtime diagnostics. No opaque cache insertion or warning suppression was used.

### Server Admin work remains separate

The requested Admin tab, server-authoritative GUID rights, kick/revoke controls
and roster/map shields are not implemented/enabled by this candidate. The
existing Helpy whitelist and self-targeted forwarding were audited; they are
not proof of native server-admin status. The user has supplied the native
`RSDragonwilds/Saved/Config/WindowsServer/DedicatedServer.ini` location and a
configuration sample. Installed mappings confirm the native account-ID and
privilege structures: mask 14 is ordinary Build/OpenChests/Chat, and native
account IDs must not be confused with RuneSchema character GUIDs. Runtime
identity conversion, owner precedence and the capability/receipt protocol
remain outstanding; no server configuration or permissions were changed. See
[Admin protocol design and remaining work](HELPY-ADMIN-DESIGN.md).

## Schema parity, RSMK workspaces and shared icon packs — installed, launch pending

Candidate: `RuneSchema-0.7.8.0-alpha-schema-parity-icon-packs-r4-20261015.zip`.
This supersedes the local r1/r2/r3 candidates for this pass and the previously
installed `source-cache-authoring-r3` build below. Installed on Steam and Game
Pass on 2026-10-15 after explicit user authorization. No push, game launch,
save edit or stock UE4SS host change was performed for this candidate.

- RSMK branding integrated from the UX handoff. The sidebar R follows the app's
  current accent immediately (including gold); brackets stay unchanged.
- Clone cards use the selected appearance's resolved native icon, with explicit
  overrides and native preparation/creation receipts taking precedence.
- Item Bay detaches to a movable native desktop window. Cross-window drag/drop,
  Dock back, close-to-dock and shared selection state are covered by the actual
  production renderer; no vendor ImGui docking/host modification is required.
- Recipe discovery merges loaded, mounted subclass metadata, cooked declaration
  and schema definition sources. The 32,768-entry rejection is removed;
  search/deduplication precede bounded paging and unavailable entries stay gated.
- Schema-created wearables enroll after validated identity registration.
  `$WearableBridge` supports explicit schema patch enrollment. Loose registry
  documents accept the eleven native lanes, with explicit `Source: Schema`
  provenance and the same required-lane cleanup safeguards as cooked content.
- Settings exposes import/export of optional `.rsmk-icons` packs and native file
  drops. Background SHA-256 source checks permit path/timestamp-independent
  matching; changed/missing sources skip independently. Validated raw pixels are
  rekeyed locally without item registration, mod enablement, or save changes.
  Cancellation, size bounds, no-junction targets and worker shutdown are covered.

See [workspace changes](HELPY-SCHEMA-WORKSPACES.md),
[shared cache packs](HELPY-ICON-CACHE.md#optional-shared-icon-packs),
[asset patches](reference/loaders/assets.md) and
[native registry declarations](reference/loaders/registry.md).

Verification: **177/177 core tests**, **29/29 standalone UI tests**, and
**66 schema positive/negative checks**. Includes portable pack round-trip across
WinGDK/Win64 fixture paths and timestamps, changed/missing-mod exclusion, corrupt
pixel/manifest rejection before publication, budget/cancellation checks,
responsive previews, detached Bay drag/drop and accent-logo texture reuse.
The canonical UPX package passed compression tests, extracted hidden/server
desktop lifecycle checks, and all 269 core imports against both installed stock
hosts. Both host DLL hashes remain
`D936FD2D66AE9287F0B32AEC5F698AA81FF20EF744EAC43BF62D8475469393DF`;
both installed core and Helpy hashes now match this candidate.

Installation: 32 framework files verified per install; all 259 Steam and 199
Game Pass protected mod/settings files unchanged. Game Pass's launch-path alias
matches. Backups in the shared workspace:
`.cache/install-backups/steam-full-alpha-20261015-03f98ba03fac44bc9be5284ededccb11`
and `.cache/install-backups/gamepass-full-alpha-20261015-acef50335a014bb2b54ab6a68f14c039`.
The server and unrelated private test probe were left untouched.

- ZIP SHA256: `1E8C564F359315B194CF51FAE9603B4F269E11D5A46C2552E385B06861C55E63`.
- Compressed core SHA256: `4106346AC4995407E29809BFA5BA6B27DF73AA0364342BCAEE826DD9DADFAC24`.
- Compressed Helpy SHA256: `5719A69EEF722C507925F505DED9CED67C9FF4533A264BC7D1F9842728C3F6BE`.

Live-game item-icon/clone appearance, recipe coverage, wearable refresh and
multiplayer enrollment still need acceptance tests. JSON cannot create missing
mesh/material/Blueprint content. These changes do not establish a shrine-crash
fix. No actual shared pack has been published; export is intentionally driven by
the author's captured, indexed icons rather than a bulk runtime texture sweep.

## Source-aware icon cache and desktop Authoring Tools — installed, launch pending

Candidate: `RuneSchema-0.7.8.0-alpha-source-cache-authoring-r3-20261015.zip`.
Both RuneSchema and Helpy DLLs use canonical UPX compression and pass UPX tests.
Installed on Steam and Game Pass on 2026-10-15 after explicit user request.
No GitHub push, live-game launch, save edit, or UE4SS host change was performed
for this candidate. Earlier local r1/r2 packages are superseded by r3;
r1's sandbox-blocked compression is not the deliverable.

- Icon captures now live in source/fingerprint subfolders in AppData. Source
  metadata is copied from successful native registration and compared by a
  filesystem-only worker; cache files never add catalogue entries or register
  missing mods. Verified native item-to-icon relationships support icons from
  a different mount without spending retries while the manifest updates.
- Simple and advanced Settings expose Clean Icon Cache and Open cache folder
  (Windows File Explorer). Cleanup is limited to Helpy's two cache targets;
  junction/redirection checks prevent following an unrelated directory tree.
  Captures remain demand-driven rather than loading all game assets at launch.
  See [Helpy icon cache](HELPY-ICON-CACHE.md) for limits and manual cleanup.
- Authoring Tools is a dedicated Helpy navigation tab. The audited legacy
  authoring UI is removed; schemas, captures, traces, profiles, diagnostics,
  presets, NPC import, effect tests, results and offline cleanup are accessible
  through the desktop routes. Native engine work remains on the game thread,
  behind explicit session/config/readiness and per-action safety gates.

Verification: 175/175 core tests; 28/28 standalone UI tests, including cache
fixtures, authoring protocol/transport, three responsive preview sizes, and
server-desktop lifecycle. The extracted compressed package passes standalone
load/lifecycle checks and all 269 core imports against both installed stock
Steam and Game Pass hosts. Both installed UE4SS.dll hashes remain
`D936FD2D66AE9287F0B32AEC5F698AA81FF20EF744EAC43BF62D8475469393DF`.
Live mod-icon browse/restart, engine-backed authoring actions and full-world
behavior remain unvalidated. These changes do not establish a shrine-crash fix.

Installation: 32 Steam and 33 Game Pass framework files verified against the
immutable package. All 259 Steam and 198 Game Pass protected mod/settings files
remained unchanged; the Game Pass launch-path alias matches. Backups:
`.cache/install-backups/steam-full-alpha-20261015-c69930f128b847768bcc960d7bd0f6b4`
and `.cache/install-backups/gamepass-full-alpha-20261015-4fa8aac8b67140a991caee521a008ca2`
in the shared workspace. Both stock hosts were hash-checked unchanged after
installation. The server and the separate RSMK branding candidate were not installed.

- ZIP SHA256: `06312BECCEE266CF571B3C0482D94AFFC3322B740C820B625ACE64CF72E3D91A`.
- Compressed core SHA256: `4F1286A5B36D253C858FF2BD2E6983BBE13C989A8675C2CEC9F0565CA097557C`.
- Compressed Helpy SHA256: `A7D07CB5278DD4A370E8CCA2BA450B3CA9ED4E7F6934269F44A886B65179AFA1`.

Game Pass shrine crash review: the 2026-10-15 17:24 UE4SS minidump records a
native GameThread access violation writing address 0x3A6, at executable RVA
0x1AD10F6. A matching CL248854 cached executable was verified by PE timestamp,
image size and checksum before unwinding. No RuneSchema or Helpy frame appears
in the fault unwind; this does not exonerate either component or identify a
cooked-content cause. The user's shrine-opening observation is a reproduction
lead, not attribution. Public-export labels are not reliable function names.
SHADWOMANCER received the separate consumable-pack feature request; no unproven
crash blame or private crash files were sent.

## Processing-station batching candidate — installed, Game Pass startup checked

Candidate: `RuneSchema-0.7.8.0-alpha-processing-batch-r2-20261015.zip`.
This targets the remaining approximately 5-second placement phase measured in
the previous Game Pass run, not its separate 6.65-second native asset-loading
phase. Installed on Steam and Game Pass on 2026-10-15: the core DLL and two
compression/mapping metadata files per installation. Backups retained; stock
UE4SS, Helpy and protected mod/config files verified unchanged (288 Steam,
227 Game Pass). No saves were edited by the installer. No server installation
or GitHub push was performed.

Game Pass reached its main menu at approximately 17:02. The loaded core module
matches the candidate hash below. All 46,159 recipes and 41,407 placements
reported zero errors, with no processing-batch rejection. All six sealed network
registry counts/fingerprints match the preceding launch. Both nested Lua
packages started; wearable and save-cleanup boundaries report ready. The existing
Flintlocks refresh-hook error, UE4SS Tick address warning and InventoryFavorites
inactive sort bridge remain unchanged. Steam was hash-verified but not launched;
no world was entered by the agent, so world/save behavior remains untested here.

Measured comparison: native reference preparation 6,654 -> 7,098 ms;
finalization 6,097 -> 6,221 ms; combined recipe work 12,751 -> 13,319 ms.
**No measured overall startup improvement on this run.** Placement took 5,004 ms:
24,169 crafting calls account for 4,870 ms, while 21,993 processing calls account
for 26 ms. Counters report 21,977 processing appends, 71 snapshot reads,
22,048 verification reads, nine destination tables and 47 ms table resolution.
These observations identify crafting placement and native loading as the next
profiling targets; they do not establish a controlled processing-only speedup.

Backups: `.cache/install-backups/steam-processing-batch-20261015-b96e6798201c425ca97f48fb511fba7b`
and `.cache/install-backups/gamepass-processing-batch-20261015-9544d0edd7a24233b0f40a6755e4d25a`.
The latter preserves the preceding launch log. Validation and fresh log copy:
`.cache/processing-batch-live-validation-20261015.json` and
`.cache/processing-batch-live-20261015.log` in the shared workspace.

- Processing appends use an initial native-array snapshot and membership set,
  immediate count/tail-pointer readback for each append, and full ordered native
  readback before batch certification. Existing nulls, duplicate native slots
  and authored append order are preserved. Named replacements verify preceding
  appends, retire that ledger and use the original conservative replacement
  path; unbatched/late DataTable replay retains the original implementation.
- Destination tables resolve once per pass into weak handles before opening
  row-memory batches. Recipe replay triggered during that table-preparation
  scope is deferred to the ensuing complete placement pass. Missing/destroying
  or not-ready tables remain failures, not silently successful placements.
- Batch contexts are stack-owned and thread-local, including crafting-category
  membership. Serialization advances an atomic revision instead of touching
  another thread's batch. Stale row ledgers are discarded without dereferencing
  old addresses; losing unverified writes refuses certification. Verified/no-op
  entries can invalidate without falsely reporting a pending write failure.
- Full-readback or processing-placement failure marks the processing batch
  invalid for new automatic grants and rejects cleanup certification. Existing
  learned/save progress is not revoked. This is verification batching, not an
  atomic rollback guarantee for already applied native DataTable writes.
- Additional diagnostics split crafting/processing calls and elapsed time,
  destination-table resolution, snapshot/verification reads and actual appends.
  Reflection metadata caching/native asset-loading optimizations are separate
  future work, not included in this candidate.

Verification: 175/175 core tests; 41,407 category plus processing appends;
preserved native nulls/duplicates, complete ordered readback, corruption/reorder
and foreign-size/storage rejection, replacements and pending-write invalidation;
native build; canonical verified UPX compression; all 269 core imports against
both original hosts; extracted Helpy hidden-window/server-desktop lifecycle;
no UE4SS.dll in ZIP and no retired custom-host API in the core. Helpy is unchanged.
Synthetic timings are not game startup timings.

- ZIP SHA256: `38C17440B0122BB6630F4E8A4217310509FD74BE24D0A9242DDCB657C44FD2B4`.
- Compressed core SHA256: `53E0207182E761EFB7909D78E244843EBA1403EF3370EF534BE1F9EFAC709AA5`.
- Helpy SHA256: `010E092E7EF9DE90ED2C25E4AC1CB0A5E68F32CF74FD1A0080119F59C5B1BACF`.

## Recipe startup performance candidate — installed, Game Pass launch checked

Candidate: `RuneSchema-0.7.8.0-alpha-recipe-batch-performance-r2-20261015.zip`.
This framework-only package does not contain or require a modified UE4SS.dll.
Installed on Steam and Game Pass on 2026-10-15 after normal title-screen closure
of Game Pass and process/lock checks. Three files per lane changed: the core DLL
and its compression/mapping metadata. Stock UE4SS, Helpy and protected mod/config
files verified unchanged (288 Steam, 227 Game Pass). Saves were not edited by the
installer. No server install or GitHub push was performed.

Game Pass launched to its title screen at approximately 16:27; no world was
entered. All 46,159 recipes and 41,407 placements reported zero errors. Item,
recipe, quest, spell and equipment-effect registry counts/fingerprints match the
baseline; MaxCape and Flintlocks Lua entry scripts both started. The previously
observed Flintlocks tooltip refresh-hook error and UE4SS Tick address warning
remain; they are not new recipe-loader errors.

Measured comparison: preparation 6,725 -> 6,654 ms; finalization 6,074 -> 6,097 ms.
Total measured recipe work 12,799 -> 12,751 ms: **no meaningful improvement on
this run**. Do not present the synthetic test as an in-game speedup. New counters
show 16 snapshot hits, 46,057 native load requests, property work 294 ms,
discovery 842 ms, placement/hook phase 4,958 ms, 4,739 category collection reads
and 154 membership builds. The membership optimization is exercised, but native
loading and remaining placement work still dominate and need further profiling.

Pre-update backups and receipts under the shared workspace:
`.cache/install-backups/steam-recipe-batch-20261015-8630edf95d9846cbb235349ec4843a87`
and `.cache/install-backups/gamepass-recipe-batch-20261015-815678eda9cc4df2a1b98f48a0dbd672`.
The Game Pass backup also preserves the prior launch log for comparison.

Baseline Game Pass startup on stock UE4SS (2026-10-15, 16:08): 46,159 recipe
definitions, 41,407 placements, 21,982 discovery rules. Reference preparation
took 6,725 ms; finalization took 6,074 ms. Placement followed discovery from
16:08:41.640 to approximately 16:08:46.557. This is observed phase timing, not
a CPU profile or a promise of the same duration on another machine/cache state.

- Recipe-reference preparation snapshots loaded RecipeData paths into scoped
  weak handles. A cache miss/stale/not-ready object still uses native loading
  and ready/type validation; the engine can reuse dependencies loaded since the
  snapshot. This avoids a public find-path scan before every missing asset load.
- Crafting-category placement indexes existing soft-reference package/asset
  identities once per batch. Appends retain authored order, validate the live
  count and appended path before updating membership, and do not resolve soft
  references into weak pointers. Storage/count changes rebuild the index;
  DataTable serialization explicitly invalidates same-address/same-size edits.
  All batch caches are discarded at scope exit, including exceptional exits.
- Processing-array clone/identity/retention checks remain intact. An incompatible
  category element layout now fails verification rather than reporting success.
  Native registry capacity, cleanup gating, discovery and unlock policies remain
  unchanged. No recipes or placements are skipped to improve timing.
- New diagnostic phase timings separate property work, discovery and placement;
  preparation reports snapshot hits and native-load requests. They are needed
  to measure actual game startup improvement on the next candidate launch.

Verification: 175/175 core tests; synthetic 41,407-placement test with one
initial membership scan, duplicate suppression, relocation/foreign size change
and in-place serialization invalidation; stale/missing/wrong-type reference
tests; native build; canonical verified UPX compression; all 269 core imports
against both original installed hosts; extracted Helpy hidden-window and
server-desktop lifecycle; no UE4SS.dll in ZIP or retired host API in core.
Synthetic timing is not in-game timing; the live comparison is recorded above.

- ZIP SHA256: `357382834EE5990D5B38CF1545D9DC6AFE30DC2D74D75F14565A9835ABADFCDC`.
- Core SHA256: `4350490D1800FDE74658D7C415EAAA12CE7F2DE59C667E1902039DCBFFDB3A61`.
- Helpy SHA256 unchanged: `010E092E7EF9DE90ED2C25E4AC1CB0A5E68F32CF74FD1A0080119F59C5B1BACF`.

## Stock UE4SS boundary — correction, 2026-10-15

RuneSchema updates must not replace UE4SS.dll. The custom-host build and bundle
entry points are now blocked; all existing `*-managed-host.zip` archives below
are historical and must not be installed. Framework-only registry fixes remain
available. Nested Lua add-ons use RuneSchema's bootstrap on stock UE4SS.
Native DLL support is outside this feature's scope; entire native/hybrid
packages are skipped with warnings, not partially executed. The core no longer
looks up or calls the retired host API. Lua packages keep their own scripts,
config files, helper modules and original paths; they are not separate host VMs
or independent ModRef instances.

Installed on Steam and Game Pass after confirming both clients were closed:
`RuneSchema-0.7.8.0-alpha-stock-host-lua-r2-20261015.zip` (framework only).
Each lane received four RuneSchema runtime/metadata files plus its original
backed-up UE4SS.dll. The matching Helpy DLL was unchanged. Protected mod/config
files verified unchanged: 258 Steam, 197 Game Pass. Saves were not touched.
No server update or GitHub push was performed. In-game validation is pending.

- Framework ZIP SHA256: `6D9B69FC32686FE364E68300C60048ADC82D1B84089358222D65FEFE8C09A3A6`.
- Compressed core SHA256: `79CBDA85C56B3114514BF3D757635E6B44B75DA9DA2A05F5785C344B6B5198B8`.
- Original host SHA256, both lanes: `D936FD2D66AE9287F0B32AEC5F698AA81FF20EF744EAC43BF62D8475469393DF`.
- Verified canonical UPX compression, 174 core tests, actual Lua package execution
  with config/module isolation, callback context, no duplicate startup, native
  and retired-host rejection, stale-manifest protection, MaxCape syntax only,
  all 269 core imports against both original hosts, no retired API string in
  the core, no UE4SS.dll in the archive, custom-host build/bundle guards, and
  extracted Helpy hidden-window/server-desktop lifecycle.

Restoration/update receipts and pre-change backups:
`.cache/install-backups/steam-stock-host-lua-20261015-bb055e067fb6419fb9ea54933ce31bdb`
and `.cache/install-backups/gamepass-stock-host-lua-20261015-e0e89b128dc5460085a2abfe8c0b388f`
under the shared workspace. The older original-host backups remain intact.

## Registry counting, large catalogues and quiet diagnostics (included in installed Lua-only correction)

The historical registry candidate was
`RuneSchema-0.7.8.0-alpha-registry-routing-diagnostics-r2-20261015`.
Its registry/catalogue/diagnostics changes are retained in the installed Lua-only
framework above. Do not use its obsolete combined managed-host bundle.

- The canonical network plan now reserves slots only for objects missing from
  the audited network registry. Already registered candidates still undergo
  full identity/collision validation and preserve their IDs. The existing
  65,535-entry capacity remains unchanged and is shared by the planner, audit
  and append guard. The reported 36,766-item and 47,411-recipe follow-up passes
  no longer count their existing entries twice. Real overflow includes accurate
  existing/new/maximum counts in its error.
- Removed the unrelated 32,768-item gate from recipe routing and item-roster
  discovery/search. Full object-path recipe references resolve without scanning
  the catalogue. Legacy PersistenceIDs build one complete index per batch;
  empty/failed results are cached, ambiguity stays rejected, and paths rather
  than unleased object pointers are retained. Resolution verifies the live
  ItemData type and PID. Timed-processing clone checks remain fail-closed.
- Confirmed live runtime-clone paths form an early, bounded-search segment.
  Their ordinary live-object entries are suppressed as duplicates, not removed
  from discovery. Mounted/native coverage and existing cursor/time budgets stay
  intact. Cape and native WearableEquipmentData entries classify as Armor.
- Removed individual PAK-registry identity announcements and wearable
  DISCOVERABLE/server-item announcements. With `advancedLogging=true`, bounded
  summaries give added/existing counts by content root, once per settled result.
  Content roots are source hints, not claimed PAK filenames. Normal operation
  stays quiet and genuine failures remain visible. Optional counters do not
  govern enrollment or cleanup. Wearable membership uses weak-validated indexed
  lookup rather than repeatedly scanning its tracked vector.

Verification: 176 core tests and 25 Helpy tests passed, including 40,000-item
recipe routing, 66 repeated vendor requests, duplicate/stale/empty/failed
lookup cases, full-capacity network boundaries, priority-cursor layout and
40,000-event aggregate-log fixtures. All 269 core imports resolve against both
installed hosts. Canonical UPX integrity, all 41 combined-bundle file hashes,
extracted compressed UI startup/shutdown and dedicated-desktop contracts passed.
The matching host and Helpy binary are unchanged from the installed candidate.

The Game Pass log confirms the real MaxCape Lua package started and later
reported `cape item found`. Its absence from Helpy was a catalogue observation,
not a confirmed failed inventory grant. The new search visibility and native
recipe/vendor outcomes still require an in-game retest; no live grant was
attempted during verification. Cooked PAK assets are enrolled once, not cloned
to work around a large roster. Third-party native DLL package validation remains
a separate pending in-game check.

| Artifact | SHA-256 |
| --- | --- |
| Combined matching-host ZIP | `AFACBDD32ACA081ED68E5638814A14F7742C12674F3A23C0095434505B144162` |
| Framework-only ZIP | `EC5DDDDCBC130B31A9576631BA3D0E5291D0D0AD904A1017C70E8F99AB2524B2` |
| Compressed RuneSchema DLL | `E42A76300A64FC642EA425C8C654096ED53691A8E59D22A68B50DE6F8E0DAFBD` |

## Complete included UE4SS packages (earlier installed candidate)

- `mods/<Owner>/ue4ss/<PackageName>/` carries whole Lua/native packages,
  including configs, metadata and dependencies. Legacy direct Lua companions
  remain compatible. No package files are copied into sibling UE4SS mod folders.
- The optional matching-host build adds a startup queue, registers genuine
  UE4SS `CppMod`/`LuaMod` instances outside the active startup iteration, starts
  included Lua after ordinary Lua mods, and tears native children down before
  RuneSchema. Parent enablement, collisions, bounded paths and explicit host
  disabled entries are checked. Restart is required for package changes.
- Framework-only updates retain compatibility with older hosts: isolated Lua
  fallback is available, while native/hybrid packages are held back. The
  fallback is not an independent Lua VM and does not supply a separate ModRef.
- Build the combined candidate with `build/build.ps1 -ManagedUE4SSHost`.
  The pinned UE4SS checkout is unchanged; the host uses one generated source
  overlay and retains the existing public class layout. A full comparison found
  all 4,247 exports from each installed Steam/Game Pass host retained, plus the
  new C queue export. This is symbol/compile validation, not proof of native
  gameplay compatibility or third-party DLL safety.

Verification: 173 core tests and 25 Helpy tests passed. A LuaRaw harness tested
actual fallback execution, config preservation, helper/global isolation,
deferred callback context, no double startup, stale-manifest rejection and
native/hybrid holdback. MaxCape's installed main.lua was read for syntax only,
not executed or changed. Production registration includes also ran against a
small host model; actual native package loading still requires an isolated
in-game startup/shutdown test. No game was launched for these checks.

The canonical UPX-compressed framework and matching uncompressed host are in
`RuneSchema-0.7.8.0-alpha-managed-packages-20261015-managed-host.zip`.
All 41 bundle-file hashes were checked; extracted compressed Helpy hidden-window
and server-desktop lifecycles passed. All 269 uncompressed core imports resolve
against the new host and both installed hosts. No installed files or saves were
modified. Third-party ModMenu nested filesystem discovery is not implemented.

| Artifact | SHA-256 |
| --- | --- |
| Combined ZIP | `E7A2E4A083582BD0C4788B909B11CEDA8DE7B0B6C89A185865A6FC8EAF92A8A1` |
| Framework-only ZIP | `49EE0DC1A874232A1586236EFA491E551C88BBD659D385A0B35B82A900DF27E3` |
| Matching UE4SS.dll | `AD770D91D8D3F43709197A0EAD03972F31ECC939D92AE1CF6317F159E23D3345` |
| Compressed RuneSchema | `776F7473B2A20F1C2EE47005048518A5248BC7BB3D43D9D7C7A5830967369DAA` |
| Compressed Helpy | `010E092E7EF9DE90ED2C25E4AC1CB0A5E68F32CF74FD1A0080119F59C5B1BACF` |

See [included packages](UE4SS-LUA-COMPANIONS.md) for the author/player contract.

## Helpy branding, building browser and carry diagnostics (not installed)

- Keep RuneSchema artwork at the upper left. Show the detected runtime with a
  gold/alpha Steam or Xbox mark, host RuneSchema version and executable build
  information in the footer. Game Pass executables without a version resource
  say `Not reported`; neither a package identity version nor a stale log is used
  as a game-version substitute.
- Move community credits to the compact information bar. Its Legal notice
  reproduces RuneSchema's MIT terms for Helpy code and keeps third-party artwork
  and component ownership separate. Original supplied logo files are unchanged.
- The map building chooser expands into a responsive categorized icon browser.
  Native `BuildableItemCatalogue.Label`, `BuildableItemCollection.Label`, and
  `BuildingPieceData.DisplayIcon` were checked against the current CL-248854
  mapping. Metadata arrives in bounded 32-piece pages; search and category
  filtering are UI-side. This is a RuneSchema rendering of native catalogue
  data, not an embedded native build-menu widget.
- Ground-unavailable placement is explicitly blocked with a retry action, not
  silently ignored. Outcomes distinguish a verified live building, a saved but
  inactive definition (do not create duplicates), and failure. Confirmation
  reports actual actor coordinates when a live actor exists. Native world,
  actor-binding and existing-native-instance proof gates remain in force; no
  unsafe construction bypass was added.
- Carry capacity retains existing schema/load-order semantics. Logs now verify
  native readback and report conflicting successfully applied capacity rules.
  Installed user player rules and character saves were not changed. The earlier
  Game Pass log showed `3x` overwritten by a `1.4x` profile; the restart log shows
  only Bigger Inventory's carry rule applying. An in-game capacity regression
  beyond that conflict has not been proven.

## Requiem event follow-up (installed on Steam and Game Pass)

Package: `RuneSchema-0.7.8.0-alpha-event-trophy-footprint-20261015.zip`.
It includes the preceding wide-map/journal/cell fixes. Both installs were
independently hash-verified and backed up; server and user content were unchanged.

| Artifact | SHA-256 |
| --- | --- |
| ZIP | `0D01755BF7C65A01AB222ADE151EBE13916B9FC5A98FA8C1A4D45C0C01B6BAEA` |
| Compressed RuneSchema DLL | `1BD315F54217CAD973AD1B06D1427CDA49DB1003154B4EDCBAF563E090BF3298` |
| Compressed Helpy DLL | `FAAE669AF716A9B7E62A8115D70FBEF9802C410FA18E7EDF060E18BABB23AF04` |

- EventOnly AI templates accept validated AdditionalDrops, include them in
  network definition matching and require the owned native AI LootDrop path.
  Instance bonus rows append native rewards, verify their bindings and restore
  the original row before cancellation/non-kill despawn/retirement. No manual
  damage-hook grants or resource-on-destruction fallback are used.
- Grounded encounters preflight the scaled root capsule's five-point footprint
  for the whole wave before constructing actors. The participant is ignored by
  these ground rays. Live capsule dimensions and placement are checked after
  construction; unsupported changes roll back the actor, not guess an offset.
- Authoritative definition changes preserve completed receipt history and
  cooldown timestamps. Unresolved native/legacy acceptance or exchange blocks
  migration. Ordinary active changed definitions still use the existing
  quest-scoped reset/re-accept; no automatic completed-stage/trophy fabrication
  and no whole-character reset were added.

Verified: 168 core tests, 25 Helpy tests, six new event-drop schema cases,
independently rebuilt COLORS TrophyCandidate-v2 content-contract test against
these headers, native builds, canonical UPX integrity, 269 UE4SS imports against
both installed hosts and extracted compressed Helpy lifecycle checks. The
content test covers a stuck old wave counter, mandatory sigil turn-in, rejection
of ordinary Zombie Packs, repeat/reload and unchanged eight waves/22 enemies.

Known limits: the log proves event waves 1-8 completed while quest stage messages
stop after stage 4, but the missing player-credit cause remains unconfirmed.
Native in-engine trophy loading/registration, actual once-per-death pickup,
cancel/no-drop behavior and the final scaled Zogre footprint need isolated
in-game verification. Available CUE audits of the 12 cooked packages are offline
metadata evidence, not a substitute for those runtime checks. No game was
launched, and no installed content or saves were changed by this follow-up.

## Follow-up candidate: journal isolation, cell bounds and travel feedback

Locally built after source commit `61c66ac`; not installed or pushed by this
follow-up. Final package: `RuneSchema-0.7.8.0-alpha-journals-cells-widemap-20261015.zip`.

| Artifact | SHA-256 |
| --- | --- |
| ZIP | `424F97CD32613E4B08A09789A04924BD21EB38338FF309DFA9F2DB8AD03BADC6` |
| Compressed RuneSchema DLL | `122BB24386A1652A9A198C75E50C54695A836B2B8DC9E949FD86E32EF6063454` |
| Compressed Helpy DLL | `FAAE669AF716A9B7E62A8115D70FBEF9802C410FA18E7EDF060E18BABB23AF04` |

- Recipe discovery no longer rejects a whole batch because an unrelated native
  progression row contains an empty recipe slot. Only requested references are
  detached, using reflected copies; unrelated entries, null slots, order and
  duplicates survive. Target/query validation and transaction rollback remain.
- Journal/lore unlock eligibility verifies the exact live native persistence
  identity rather than depending on the whole-character cleanup certificate.
  Unrelated enrollment failures still veto pruning, but cannot veto independently
  verified entries. Repeated identical failures from attack/inventory callbacks
  are reported once per component/entry/reason, without marking failed delivery
  complete or suppressing retries. Registry logging reports deferred cleanup
  truthfully instead of printing READY for an unpublished certificate.
- NPC and spawn cell bounds are decoded from reflected Box fields into owned
  storage, not aliased to the SDK wrapper's different alignment/padding. Cell-edge
  coordinates are inclusive. Actual visible-cell, authority, ground/collision
  and actor-identity checks remain; this is not a forced-spawn fallback.
- Helpy shows indeterminate map-travel progress with phase, elapsed time and
  pre-departure cancellation. Repeat travel skips placement-only ground previews
  and building lookups. Progress pushes cannot become completion receipts or
  free the mutation slot; native arrival still requires verified player state.
- The map fills a rectangular workspace with aspect-correct, cursor-anchored
  zoom up to 32x and Fit world. Native hover/selected-point ground-Z lookups use
  an independent, coalesced, debounced read-only mailbox below mutation priority.
  Missing loaded collision is reported as unavailable, not guessed. Ground
  replies are request/world correlated and stationary hover samples expire.

Verification: 168 core tests, 25 Helpy tests (including real transport decoding,
repeated-selection and ground-height fixtures, rectangular viewport round trips,
zoom/pan/fit checks and normal/compact/minimum rendering), 32 schema
cases, both DLL builds, canonical UPX compression/integrity, all 269 core imports
against both installed UE4SS hosts, and extracted compressed Helpy hidden-window
and dedicated-desktop lifecycle checks passed. Progress, arriving, widescreen,
selected-point Z and hover-Z views were captured with synthetic data. Progress,
selected-point Z, hover-Z and the minimum-size layout were visually inspected.

Still requires in-game retesting: journal player unlocks, actual NPC/resource
appearance at their authored cells, distant/repeated travel and safe rejection
on Steam and Game Pass. No worlds, learned progress or installed files were
modified during these checks. The separate UX Authoring handoff is not merged.

## Included changes

- INI settings with legacy JSONC migration, dedicated-desktop Helpy startup and
  environment branding; retain installed user configuration and mod enablement.
- Cooked recipe, journal and lore declarations with registry-enrollment health
  checks that veto orphan cleanup when required enrollment fails.
- Bounded conventional-root PAK registry shards, independent of Asset Registry
  enumeration, with atomic declaration publication and cross-owner/path checks.
- Native recipe discovery policies and overrides for supported item, interaction
  and skill prerequisites; native learned-state and unrelated grants are retained.
- Helpy Map destination preparation using a temporary World Partition streaming
  source, verified landing ground and the native teleport service/loading screen.
- Helpy recipe-station identification using typed native station bindings,
  building names/icons and exact recipe membership beyond the generic inspector's
  64-entry limit. Item/upgrade tables and vendor rows are not physical stations.

See [registry shards](COOKED-REGISTRY-SHARDS.md),
[recipe discovery](RECIPE-DISCOVERY.md), [map travel](HELPY-MAP-TRAVEL.md),
[recipe stations](HELPY-RECIPE-STATIONS.md) and [INI settings](SETTINGS-INI.md).

## Previous verified package (before the follow-up above)

Filename: `RuneSchema-0.7.8.0-alpha-shards-map-stations-final-20261015.zip`.
This records the locally built candidate; the ZIP is not committed to Git and
this branch push does not publish a tagged release asset.

| Artifact | SHA-256 |
| --- | --- |
| ZIP | `EDE6CC3D7AEDD1D14C0C39F54125E4AB541DB8830F817048084064E614288C65` |
| Compressed RuneSchema DLL | `3C4DAE48E559DAEBE731A8344C6B6A9A89ED45F019F14FF42FADDE32DED696CC` |
| Compressed Helpy DLL | `79822F4F2DEF8A13329291AAF52B065D9098F1EE46654E741E6AA22087EDC725` |

Both DLLs use the normal `UPX -9 --best --lzma` packaging path and passed UPX
integrity checks. All 269 core UE4SS imports resolve against both installed Steam
and Game Pass hosts. Extracted compressed Helpy hidden-window and server-desktop
lifecycle checks passed, along with 168 core tests, 25 Helpy tests and 32 schema
cases.

An independent audit using the candidate's actual shard/manifest/discovery
headers validated Colors' 80 exact declaration payloads across five roots:
21,982 recipe policies and 41,548 prerequisite references, without Asset Registry
enumeration. This checks payload compatibility, not mounted in-game enrollment.

## Still requires in-game validation

- Station textures and complete station coverage on Steam and Game Pass.
- Distant destination streaming, native travel completion, safe rejection and
  multiplayer client loading-screen behavior.
- Native discovery behavior for fresh and existing characters with the new cooked
  content; no completed test here claims universal learned-recipe backfill.

The preceding package did not contain the NPC `WaitingForCell` / zero-scan
correction. The follow-up above addresses its cell-bounds layout defect, but
still requires an in-game retest. The separate UX Authoring handoff remains a
distinct, not-yet-applied change set and is not included by either snapshot.
