---
channel: experimental
promote_to: SERVER-ACCESS.md
---

!!! info "Experimental · local candidate"
    These changes are being tested locally and have not been published.

# Connect Helpy to server authority

The host executes item delivery and teleport requests. A client uses Helpy as
a remote for its own active character; a character GUID alone grants no access.

## Set up the host once

1. Open **Settings → Server access → Host configuration** in Helpy assigned to
   the host/server installation.
2. Enable PIN access, enter an 8–64 character PIN without spaces, and enable
   client item grants and/or client teleportation. Choose the item quantity limit.
3. Press **Save host settings**, then restart that host/server normally.

The PIN, action switches, limits and optional character-GUID/name allowlists
are saved in that installation's **settings/settings.ini** and reload on later
launches. Keep this file private. Editing a client's host settings does not
change a remote server. The server does not need Helpy's window left open.

GUID/name allowlists are an alternative, broader explicit host permission.
PIN access is limited to self-service delivery and map teleportation. PIN users
are not automatically added to an allowlist. The connected player's own GUID
can be copied from the access panel when the server reports it.

## Connect as a client

1. Join the world with the RuneSchema networking bridge installed on both ends.
   The client makes one read-only access-status request on connection.
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
temporary sessions; restarting does not preserve client authentication.

- [RS-NET-006: PIN access available](../ERROR-CODES.md#rs-net-006)
- [RS-NET-007: client-server authority bridged](../ERROR-CODES.md#rs-net-007)
- [Compare the host's mods](SERVER-MOD-LIST.md)

Protocol, persistence and UI fixtures cover this flow. An authenticated live
client/server check remains a separate acceptance test for this candidate.
