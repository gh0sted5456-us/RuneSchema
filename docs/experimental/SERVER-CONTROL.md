---
channel: experimental
promote_to: servers/helpy-control.md
---

!!! info "Pre-release · 0.8.1.0"
    Included in 0.8.1.0. Live grant/revoke, reconnect and restart persistence
    need acceptance beyond policy and UI tests. [Candidate](PRE-RELEASE.md).

# Helpy server control

Enable **Server / Co-op** under Settings → Workspaces. The page uses the same
two-pane layout as Settings, with **Settings**, **Players** and **RuneSchema access**.

## Settings

Run Helpy from the dedicated server installation to review its active
`DedicatedServer.ini`. Server, World, Passwords and Files & changes tabs retain
the existing staged-change workflow. Stop the server before applying the
reviewed changes. Native admin records and unknown keys remain preserved.
Connected clients cannot edit the server's INI through this page.

## Players and native admin

The **Players** pane has **RuneSchema access** and **Native Admin** tabs.
In **RuneSchema access**, local server Helpy can authorize one player or all
connected players and save the optional **Automatically authorize connecting
players** toggle. Server-observed character identity, account name, platform and
player-ID alias are retained in private host settings. Grants persist across
reconnects and restarts; clients refresh access within five seconds. See
[saved player authorization](SERVER-ACCESS.md#saved-player-authorization).

These RuneSchema grants enable the player's server-executed Helpy actions. They
do not change the native owner or Admin role.

Join as the server's native owner and open **Players → Native Admin** in that client's Helpy.
Refresh players and roles, select **Grant Admin** or **Revoke Admin**, and confirm.
The owning-player bridge carries the request to the server. The server resolves
the actual caller, rechecks native ownership, verifies the current world and
target connection, then invokes the game's native privilege operation using
the target's account identity. Character GUIDs select the current player; they
are not substituted for the game's account identity.

The owner cannot be demoted here. Unknown contracts, stale rosters, disconnected
targets and ambiguous identities disable or reject changes. PIN access and
item/teleport allowlists do not grant administration rights. The server desktop
can view its roster; changing native roles requires the owner's connected game
client and never borrows another player's controller identity.

A submitted call is not a confirmed change. Helpy reports confirmation only
after reading the resulting native role on the server. The game owns saving
native permissions; confirmation of the current role is not a claim that
restart persistence was verified. There is no automatic mutation retry.

## RuneSchema access

This section reuses the existing [server-access settings](SERVER-ACCESS.md).
Host PIN, item and teleport policies remain separate from native Admin.

The native declarations examined for this candidate include
`DominionPlayerController.Server_GrantPrivileges` and `Server_RevokePrivileges`.
Runtime checks validate their native reliable-server flags, parameter layout,
`UniqueNetIdRepl` identity and `EPlayerPrivilege` enum before use. No raw
replicated privilege bytes or admin INI tuples are edited.
