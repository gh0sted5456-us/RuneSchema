---
channel: experimental
promote_to: SERVER-ACCESS.md
---

!!! info "Pre-release · 0.8.1.0"
    Included in 0.8.1.0. See the [candidate and validation limits](PRE-RELEASE.md).

# Connect Helpy to server authority

The host executes item delivery and teleport requests. A client uses Helpy as
a remote for its own active character; a character GUID alone grants no access.

## Set up the host once

### Saved player authorization

Open **Server / Co-op → Players** in the server's own Helpy or the native
owner's connected client. Use **Authorize player** or **Authorize all connected**.
The saved grant takes effect immediately; clients refresh their access within
five seconds, or can select **Refresh access** in Settings → Server access.

**Automatically authorize connecting players** is optional and off by default.
Enabling it authorizes current connections too. Turn it off before removing a
player's saved access; disabling the toggle keeps grants already saved.

RuneSchema records the server-observed character GUID, character name, account
name, platform and current player-ID alias in the host's private
`settings/settings.ini`, under `helpyAuthority.authorizedPlayers`. Grants reload
on restart and follow the same character on reconnect. A display/account name or
reused session player ID cannot grant access. A different character needs its
own grant. Missing platform metadata is shown as unavailable rather than guessed.

Saved authorization enables item delivery, map travel and temporary spawns
without requiring the separate PIN action switches. The server retains quantity,
duration, item-network-ID and ground checks, and executes requests for the
requesting character. It does not grant native Admin, permanent authoring,
arbitrary commands or local files. The native owner also has this RuneSchema
access without entering a PIN. Local server Helpy does not need an in-game owner
character to authorize connected players.

### PIN alternative

1. Open **Settings → Server access → Host configuration** in Helpy assigned to
   the host/server installation.
2. Enable PIN access, enter an 8–64 character PIN without spaces, and enable
   client item grants and/or client teleportation. Set the item quantity limit
   to **0** (the default) for no server total per grant request.
3. Press **Save host settings**. A running host applies the changes and sends
   the updated policy to connected RuneSchema clients.

The PIN, action switches, limits and optional character-GUID/name allowlists
are saved in the host installation's private **settings/settings.ini**. The
running host watches for edits to that file and applies valid changes. Open
Helpy assigned to the server installation to edit that server's settings.

An existing nonzero limit stays in effect until you change it. Every request
still goes through inventory capacity, item-network-ID and per-row validation.

GUID/name allowlists are an alternative, broader explicit host permission.
PIN access is limited to self-service delivery and map teleportation. PIN users
are not automatically added to an allowlist. The connected player's own GUID
can be copied from the access panel when the server reports it.

Native owner/admin roles are separate. See [Server control](SERVER-CONTROL.md)
for owner-controlled admin grants and revocations.

## Connect as a client

1. Join the world with the RuneSchema networking bridge installed on both ends.
   Helpy stores the server's access policy for the current connection. The
   server sends updates when its policy changes.
2. When the host offers PIN access, a diagnostic notice appears. Open Helpy;
   its **Connect to server authority** form asks for the world PIN. **Not now**
   dismisses it for this character/world. Use **Settings → Server access** later.
3. Enter the PIN and press **Submit PIN**. A rejected PIN grants no access; the
   response explains the refusal or retry limit. Submitted text is cleared after
   an accepted submission and is not saved by the client.
4. A successful server response shows the green **Client-server authority
   bridged** diagnostic. The panel lists which actions the host permits.

Stage items and use map travel through the normal controls. Requests travel over
the existing owning-player bridge; only the server executes them. An accepted
submission is not a completed delivery: wait for the actual result and inspect
inventory/location before retrying an uncertain operation.

## Boundaries and reconnection

The server verifies the actual owning controller, current pawn and world. It
retains quantity limits, item-network-ID verification, target validation and
teleport ground checks. A correct PIN does not bypass these checks, act on
another player, execute arbitrary commands or permit permanent authoring.

Access sessions are in memory, scoped to the owning connection and character,
and expire after an idle hour. Changing world/character, restarting the server,
changing the PIN or disabling access requires authentication again. **Disconnect**
revokes the current session. Persistent host settings are separate from these
temporary PIN sessions; restarting does not preserve PIN authentication. Saved
player authorizations above persist independently of PIN sessions.

- [RS-NET-006: PIN access available](../ERROR-CODES.md#rs-net-006)
- [RS-NET-007: client-server authority bridged](../ERROR-CODES.md#rs-net-007)
- [Compare the host's mods](SERVER-MOD-LIST.md)

Protocol, persistence and UI fixtures cover this flow. An authenticated live
client/server check remains a separate acceptance test for this candidate.
