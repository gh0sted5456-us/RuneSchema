---
channel: experimental
promote_to: SETTINGS-MENU-ACCESS.md
---

!!! info "0.9.0.7HF hotfix prerelease"
    This integration requires [RuneSchema 0.9.0.7HF and the paired Settings Menu build](../RELEASE-0.9.0.7HF.md) together.
    Signed-request, persistence and configuration tests pass locally. A real
    dedicated-server PIN unlock, restart and rejoin remain to be tested.

# Settings Menu PIN and saved Helpy access

A successful server PIN unlock in Settings Menu grants full RuneSchema/Helpy
authorization to the connected character. There is no second Helpy PIN prompt.
RuneSchema saves that character in its existing authorized-player list in the
server's `settings/settings.ini`, so authorization survives a server restart.
Settings Menu's server PIN remains its own credential; this link does not copy
the PIN into RuneSchema.

PIN approval is administrator approval for Helpy. An authorized character can use
the existing server-backed Helpy tools and manage RuneSchema player authorization.
Native game ownership and native game Admin changes keep their separate owner
checks. Item identities, quantities, delivery flags and transfer receipts still
undergo their existing checks.

## Setup and revocation

Use both updated DLLs. RuneSchema now ships `sm.json` instead of `modmenu.json`.
Its **Settings â†’ Server access â†’ Settings Menu PIN grants full Helpy access**
setting defaults to **On** on the host. Personal controls still edit the player's
own RuneSchema settings. Changing the host switch takes effect through the
existing watched settings and policy-push path; this adds no polling.

Enter the server's existing PIN through Settings Menu's **Unlock with PIN**.
Settings Menu verifies the signed request, then supplies the server's native
owning player controller to RuneSchema. RuneSchema identifies the character from
that controller, never from a client-supplied GUID, account name or display name.
The character must be initialized in an authoritative game world.

Helpy's connected-player controls can revoke saved RuneSchema authorization.
Turning the link off stops future linked approvals; it does not erase existing
approvals. Someone who still knows a valid PIN can authorize themselves again
while the link is enabled. Rotating the PIN does not revoke saved characters.

Helpy's existing successful PIN authentication also saves full authorization
instead of granting only a temporary session. Its separate opt-in and PIN
validation remain required for that entry point.

## Compatibility and failure handling

Settings Menu finds RuneSchema's optional native export only in the loaded
`Mods/RuneSchema/dlls/main.dll`. It does not load another copy. Older or absent
RuneSchema versions leave Settings Menu's own unlock working. If RuneSchema
cannot save the approval, Settings Menu reports that Helpy authorization was not
saved; it does not claim the link succeeded.

RuneSchema's `sm.json` uses the `ini-json` settings format: INI sections with JSON
boolean, string and number literals. The updated Settings Menu preserves those
types and unrelated private fields. The whole RuneSchema settings file is never
mirrored to clients. Existing RuneSchema settings are retained; the template
does not replace a player's settings file.
