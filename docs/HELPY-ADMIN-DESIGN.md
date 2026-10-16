# Helpy Admin — design and outstanding integration

Status: **read-only role badges implemented in the local player-badges candidate,
not installed or live-server validated**. The Admin tab, native owner-account
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
