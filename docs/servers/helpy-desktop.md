# Helpy on a dedicated-server desktop

The 7.8 alpha supports the separate Helpy window on a dedicated server launched
in an interactive Windows user session. It does not depend on the game's
viewport, player pawn or rendering mode. It is a local administrator tool—not
a remote control port or a client authority bypass.

When enabled in the Helpy plugin settings, the server starts its normal taskbar
window without taking focus. Click it to open it; F2 toggles it when Helpy or
the server console has focus. Closing the server window minimizes Helpy to the
taskbar so it remains reachable. Client F2 behavior is unchanged. Where hardware
Direct3D is unavailable, the window can fall back to the Windows WARP renderer.

Windows services/Session 0 have no interactive desktop. Helpy skips window
creation there and logs the reason without stopping the server. To use the
desktop window, launch the server in the signed-in user's desktop/RDP session.
This does not expose another user's desktop or manufacture a UI for a service.

Only the standalone Helpy plugin is exempted from RuneSchema's dedicated-server
client-plugin exclusion. Other client-only plugins remain suppressed. Engine
and reflection work remains on the game thread; Helpy sends bounded requests.
Player-dependent actions require an actual available player; a dedicated host
must not invent a local player or silently target a different one. Live native
icon capture also depends on the assets/rendering capabilities available to the
server; a desktop window alone does not guarantee every cooked thumbnail.

The new Authoring workspace is being integrated separately. Existing tool access
is retained until its replacement is verified. Offline save-cleanup tools export
new character JSON copies only; they do not overwrite an original or bypass
world-pruning registry checks.
