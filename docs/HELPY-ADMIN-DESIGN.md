# Helpy Admin — design and outstanding integration

Status: **read-only role badges implemented; full native-role integration still
requires live-server validation**. The Admin tab, native owner-account
resolution and grant/revoke/kick controls remain unimplemented. The earlier
clone-icon/multi-recipient candidate does not contain these badges. Do not infer
authorization from a client setting, display name, known GUID, badge or merely
having RuneSchema installed.

## Read-only live badge layer

The authoritative game thread reads the authenticated players' native
`PrivilegeComponent.Privileges.PrivilegeMask`. Bit values come from the live
`EPlayerPrivilege` enum, not a storefront/version constant or a nonzero-mask
guess. Known native owner/admin roles, local host and ordinary privileges have
distinct theme-colored identifiers. Unknown native contracts remain unbadged;
a positively verified local co-op host can still show Host when native privilege
metadata is unavailable. Owner appears only when the live native role reports
Owner; matching `DedicatedServerSettings.OwnerId` to account identity is not yet
implemented. The tooltip explicitly does not promise Helpy mutation rights.

Helpy's existing read service coalesces status hints at most once per five
seconds. The game thread services them independently of the inventory/spawn/
travel mutation slot. Listen/co-op and dedicated hosts use the same reader;
remote clients request a fresh roster through their owning-player bridge.
Remote replies must match the pending revision, component weak identity and
local world generation; old or unsolicited replies are ignored. Status is
bounded to 256 rows and contains no account IDs, INI contents or credentials.

The UI replaces the role snapshot on each accepted sequence. Badges expire
after 12 seconds without a fresh snapshot; repeated snapshots cannot renew
them. World generation changes and empty authoritative rosters clear them.
Map players join by their reflected character GUID, never display name or a
reusable numeric PlayerId. The replicated path is validated as
`OwnerGuid.CharacterGuid.InnerGuid` with native struct names/sizes checked.
Here a character GUID correlates presentation only; it does not authenticate
against the server's account-based owner/admin list. Ambiguous GUIDs do not
receive map badges. Roster controls and map markers share the same cache.

Changes made by the game's live privilege system appear in later snapshots;
this feature does not hot-reload external INI edits or assign/revoke rights.
All existing server-side Helpy action gates remain unchanged. Real multiplayer
acceptance (including owner precedence, reconnect and revocation) is pending.

## Inventory access and delegation — agreed 2026-10-21

This is requested design, not an implemented grant/edit UI or authorization gate.
The host should grant explicit View inventory, Add items, Remove items and Delegate
rights, scoped to a character or reviewed group. Players with delegated authority
may grant only a subset of their own active rights, scope and lifetime. They must
never mint host/admin status, widen targets, or extend expiry. Revoking a parent
grant invalidates every derived grant immediately; disconnect/server/world changes
invalidate session-bound authority. Audit grant, revoke and inventory mutations.

Every cross-player request must resolve the caller from the owned server RPC,
re-resolve the current target and item instance, and check the effective grant
again at execution, not just queue admission. View is not mutation authority;
Favorite/Junk ownership stays with the character owner even if another player can
edit inventory. Native item operations must have bounded quantity checks and a
final verified receipt. Never automatically retry an uncertain add/remove.

Private tags remain owner-only. Nicknames and cropped portraits may be public;
inventory contents are shared only with the owner or explicitly authorized viewers.
Public profile transport must not include arbitrary client filesystem paths or
private tags. Character GUIDs, reusable PlayerId values, display names and badges
are presentation/target selectors, not authentication credentials.

The existing native account-ID/bootstrap gap described below still blocks a
claim of safe persistent operator grants. Until that contract is implemented and
tested, keep the present server mutation allowlist in force; do not silently turn
on cross-player edits. Real two-peer acceptance must cover spoofed targets,
revocation after queueing, reconnect, stale grants and duplicate receipts.

## Intended contract

- A dedicated Admin tab for connected-player status, allowed actions and
  reviewed server operations. GUID grants/revocations belong to server policy;
  clients never edit the server INI directly.
- Explicit capabilities for item grants, temporary spawns, teleport, kick and
  managing permissions. Managing permissions needs a separately bootstrapped
  server operator; being allowed to spawn must not imply permission to grant
  oneself administrative rights.
- An authenticated owning-player RPC supplies caller identity. The server
  resolves target GUIDs to current players and checks authority on execution,
  including after queuing. Client-supplied identity/role fields are untrusted.
- Server-generated, session-bound capability snapshots and correlated final
  operation receipts. An admission/queue acknowledgment is not successful
  delivery, teleport, kick or durable policy persistence.
- A shield beside a verified player in Helpy's roster and map marker, with a
  tooltip listing server-confirmed capabilities. Unknown/stale status stays
  unbadged; revocation, disconnect, world/server change and expiry clear it.
- Server policy writes are bounded structured changes to approved keys, with a
  backup, atomic publication and conflict detection for external INI edits.
  Failed saves must not be reported as persistent grants. Revocations need
  immediate enforcement, not only a cosmetic badge refresh.
- Explicit confirmation and audit results for kicks/revocations, bounded
  request size/rate/replay handling, and native function-signature validation.
  No arbitrary file paths, console strings, reflected function calls or scripts
  are accepted from the client.

## Existing behavior verified in this source

`helpyAuthority.permittedPlayerGuids`/`permittedPlayerNames`, plus global grant
and temporary-spawn switches, already gate remote Helpy mutations. They are not
proof of the game's native server-admin role. The existing server handler
replaces the supplied target with the authenticated caller and returns queue
acceptance. The read-only badge extension now adds native role presentation to
its roster, not a Helpy action capability record. That mutation protocol must
still be extended rather than reused blindly for other targets.

## Native dedicated-server source — verified 2026-10-15

The user supplied the native relative location and a configuration sample:

```text
<server-root>/RSDragonwilds/Saved/Config/WindowsServer/DedicatedServer.ini
[/Script/Dominion.DedicatedServerSettings]
```

Resolve this from the running server's game/saved directory, not the desktop
client installation, process working directory, or a hardcoded SteamCMD folder.
The sample's network-share location is not an installation destination. A full
managed server installation path has not been supplied; no server deployment is
authorized or performed by inspecting the sample.

The sample contains `OwnerId` and Unreal-serialized `KnownPlayerList` entries
with `UserId`, `UserName`, `Privileges.PrivilegeMask` and `bIsBanned`. The installed
Game Pass mapping also verifies these native types:

| Native type/property | Verified representation |
| --- | --- |
| `DedicatedServerSettings.OwnerId` | String; separate ownership record |
| `DedicatedServerSettings.KnownPlayerList` | Array of `ServerKnownPlayer` |
| `DedicatedServerSettings.AdminList` | Array of `DediServerKnownAdminsIds` |
| `ServerKnownPlayer.UserId` | `UniqueNetIdRepl`, not a character GUID |
| `ServerKnownPlayer.Privileges` | `PlayerPrivileges` with a byte `PrivilegeMask` |
| `PlayerState.UniqueID` | `UniqueNetIdRepl` |
| `DominionPlayerState.PrivilegeComponent` | Native object containing `Privileges` |
| `DominionPlayerState.OwnerGuid` | `DomOwnerGuid`, with separate `CharacterGuid` and `AccountGuid` |

The mapping's explicit `EPlayerPrivilege` values are `None=0`, `Admin=1`,
`Build=2`, `OpenChests=4`, `Chat=8` and `Owner=128`. Thus `PrivilegeMask=14`
is Build + OpenChests + Chat, **not Admin**. Do not treat any nonzero mask as
administrator status, or classify the configured owner solely by that mask.
These values were read from the installed mapping; live privilege replication,
ownership precedence and account-ID conversion still require runtime validation.

RuneSchema's existing `GetPlayerCharacterGuid` calls `GetCharacterGuid`.
Although an account ID and character GUID may both print as 32 hexadecimal
characters, that does not make them interchangeable. Never compare a server's
`OwnerId`/`KnownPlayerList.UserId` to a character GUID or use a display-name match
as a new native-admin authentication path.

For the integration, prefer validated native reflected settings and the
authenticated player's live privilege component to brittle offsets, raw
`UniqueNetIdRepl` memory reinterpretation, or guessing from INI strings. Unknown
types/identities/role state remain unbadged and unauthorized. Native admin
status and RuneSchema-only action capabilities remain separate; the latter must
still obey server policy and execution-time checks.

This native file is **not** RuneSchema's `settings.ini`. Its Unreal tuple/array
serialization must not be fed through the generic RuneSchema INI codec or
rewritten wholesale. Native structures also contain `LastAdminPassword`, and
the file can contain world/admin passwords. Never expose the file, credentials,
raw known-player list or password fields in Helpy snapshots, diagnostics or
shared fixtures. The source sample was inspected read-only; no credentials,
admin list, server configuration or rights were changed.

Acceptance requires the real server counterpart: denied ordinary caller,
authorized operator, no self-escalation, replay/forgery rejection, target
disconnect, durable-save failure, immediate revocation, badge expiry and a
second player/map observer. Include a regular mask-14 player, a separately
configured owner, and distinct account/character IDs. A client UI fixture or
offline mapping inspection cannot establish those runtime claims.

### Runtime declaration and mapping diagnostics

Mods shows registrar-owned accepted/rejected declaration counts and bounded
source/reason details from the connected runtime. Acceptance means a declaration
was parsed and enrolled; it does not certify every native reference or gameplay
operation. An optional conventional cooked-root lookup can miss when a mod uses
loose JSON or another cooked asset path. Those two missing-path probe warnings
remain in Live Log but do not alone mark the mod unhealthy. Registrar rejections,
other warnings, and errors are still reported. Explicit mod ownership takes
precedence over dependency paths mentioned in a log message.

Automatic reflection mappings resolve the host-provided runtime directory by
handle and verify its identity before using its canonical path. The Game Pass
WindowsApps package junction is therefore supported without allowing redirects
inside the mapping cache. Generated replacements must pass the existing native
type and fingerprint checks before activation and retirement of old generated
files. Shipped/custom mappings and the last working fallback remain protected.
Mapping compatibility v3 is rechecked using the game executable/version and
native game/feature containers. RuneSchema/UE4SS DLL updates, managed mod
definitions and load order do not trigger a new capture. A valid matching
mapping is reused; missing/invalid mappings or changed native game inputs
request replacement. Runtime caches and artwork are not mapping identity inputs.
This is a bounded identity check, not a claim to detect every external patch or
reflect every unloaded Blueprint. Native dump capability, permissions and validation failures
retain the fallback and report the specific reason instead of accepting an
invalid replacement.

Home and Profile default below the navigation divider, with Report a bug;
Settings defaults above it. All rows and the divider retain one drag order and
matching column geometry. Home centers the runtime badge above its social links.
Party placards show three compact critical meters above interactive
equipped items; advanced display settings retain the wider row layout. Food,
hydration, rest, and skill levels are the only member-hover details. Equipped
items retain their own hover details and right-click actions on the placard.

Change gear opens a focused Item Bay without the inventory pane or unrelated
category preferences. Verified matching rows already cached for the current
runtime appear immediately; remaining pages use the shared backend catalogue.
Owned-item swaps retain native instance, slot and host-authority validation.
New items are staged through the existing delivery cart, not granted implicitly.
Item previews use the reflected SkinnedMeshComponent.SetSkinnedAssetAndUpdate
contract; preview calls still require native, non-networked functions and checked
owner, parameter types and extents. No gameplay item Blueprint is spawned.

The Buildings panel can submit world X/Y coordinates in Unreal centimetres to
the same TargetLocation path as map placement. The runtime checks native map
bounds and traces loaded ground; Height offset controls the vertical offset.
Unknown ground or a failed binding check produces no placement. Permanent
placements remain available through the World map placement workflow.

Helpy focuses on its desktop tools and does not replace or restyle the game's HUD.
RuneSchema's native console menu commands remain available independently of Helpy.

Players → Character Editor → Helpy Storage follows the verified local character
GUID automatically and refreshes stored items asynchronously. Offline editing
is separate; it does not need to be activated to view the current character's
Storage. An unverified or ambiguous player identity never selects another bin.

The character picker reads Steam/Epic JSON and Xbox WGS character JSON payloads,
keeping the newest Xbox payload per character. Opening an Xbox character creates
a separate working copy. WGS blobs, provider indexes and world containers are
never rewritten by this editor; exporting JSON is not a WGS attachment operation.

With Home → Scrap refunds enabled, the native item description displays
“Hold S for Scrap” for an eligible hovered inventory stack. A fresh one-second
hold queues the existing verified refund/deletion action. Selection, focus or
world changes cancel the hold. Equipped/protected items and ambiguous recipes
remain unavailable. Native X/Delete is unchanged and is never replaced by S.

Storage's right-click Restore action sends a one-shot request to the current
local character on the authoritative runtime. The runtime rebuilds only a fresh,
unowned transient item, verifies its saved class/schema and original instance,
then uses native AddItemToSlot into an empty valid slot. Durability, tags,
upgrades and other saved fields are preserved; the game may renew its inventory
timestamp. A durable restore intent precedes insertion. Uncertain results remain
blocked from retry, and no already-owned item is rewritten or merged.

Favorites/Junk are built in and enabled by default, with no Home toggle. The
staged small native inventory marker cycles Unmarked → Favorite → Junk →
Unmarked for that exact character's stack. Item-type marks remain available in
Helpy's Organize menu. The native marker and Helpy use the same local marks store.
Only the marker handles its click; vanilla Use/Equip, item clicks and X/Delete
remain unchanged. There is no extra popup/numbered menu. Junk does not initiate
Scrap or deletion. Cross-machine synchronization and native Favorite protection
are not implied. Native marker interaction still requires in-world acceptance.

Item Bay categories use native gameplay tags before names. Building resources
(including Stone and ores) are Resources; rune ammunition is Runes, while rune
weapons and tools retain their equipment categories. Unloaded references use
whole words from the asset leaf, not mod-folder substrings. Cached, imported,
live and editor rows use the same rules, including old cached category labels;
no icon scan or full cache rebuild is required. All Item Bay category selectors
expose the same categories. Items loaded after a native class-index snapshot
remain discoverable through mounted metadata, without retaining raw UObjects.
