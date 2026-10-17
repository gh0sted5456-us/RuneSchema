# Helpy on a dedicated-server desktop

RuneSchema/RSMK 0.8.0.0 support the separate Helpy window on a dedicated server launched
in an interactive Windows user session. It does not depend on the game's
viewport, player pawn or rendering mode. It is a local administrator tool—not
a remote control port or a client authority bypass.

The server-only **Dedicated Server** navigation entry edits the local active
`WindowsServer/DedicatedServer.ini`, with masked secrets and reviewed scalar
changes staged for offline application. The live INI is not overwritten:
stop the server and run the supplied apply helper, which checks edited-field
baselines and saves with a backup. Confirm the active profile's path first.
Changes are not live permission grants. See the
[server configuration editor guide](dedicated-setup.md#edit-dedicatedserverini-in-the-servers-helpy).
**Live Log** is available on both clients and server desktops for filtering,
searching and locally exporting this installation's RuneSchema diagnostics.

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
must not invent a local player or silently target a different one.

The catalogue reads mounted registry metadata and live ItemData, armor,
building-piece, AI, NPC and resource definitions independently of texture
capture. An empty player roster does not prevent item/AI/resource queries or
full indexing of the loaded authoritative world. Newly starting mounted
registries are retried rather than cached as an empty world-long result.
Bounded metadata queries also run before the spawn loader captures its first
ready world; accepted reads no longer wait forever on that unrelated context.
If definitions are not ready, a correlated error completes the request so it
can be retried, rather than leaving a loading spinner stranded. Mutation and
travel requests still require their own world/player/authority checks.
Connected-player grants and travel still require a real possessed player;
world placement still requires loaded collision and the normal server checks.
A startup/lobby map is not proof that the configured playable world loaded.

On a dedicated server, native game-GPU icon capture is disabled; desktop
rendering, including WARP, remains enabled. Source-scoped cached pixels are
tried before any native capture gate. Import `.rsmk-icons` packs through
Settings or drop one onto the window. The server-only **Use client packs as
display-only thumbnails** option matches exact icon paths in active server
source groups even when client/server cooks differ or server textures are
stripped. Missing source groups are not activated. Checksums, pixel dimensions,
path validation and cache quotas remain enforced. These visuals may be stale;
they are marked display-only and excluded from verified cache exports.

Packs contain pictures, not live item definitions, permissions, mods or saves.
An imported image cannot make an unregistered item grantable or an unavailable
NPC spawnable. If the server definition lacks an icon reference as well as its
pixels, the UI keeps its text/details and placeholder rather than guessing a
picture from a similarly named item.
Grid cards, list rows, selected details and cart/drag labels now share the same
name policy: use a valid game Name/DisplayName first, otherwise a readable
asset/internal label. Empty, null and missing-string-table names do not produce
blank rows. A fallback label is not a claim that localization was available.

Item browsing is A-Z before pagination, including dedicated-server rows with no
textures. The first ordered query scans metadata in bounded game-thread slices
and shows Preparing A-Z list; repeated queries reuse bounded native/UI caches.
World, source and authored-item changes invalidate the native ordered index.

Cart recipients can be removed individually or cleared without changing item
quantities. A world transition drops the old roster/selection and rejects
late old-generation roster and receipt pushes. Disconnected players are removed
by identity, not name; remaining batch targets are checked before submission.
An interrupted/unconfirmed grant is never automatically replayed. Inventory
review remains required before retrying such a delivery.

All peers now defer startup clone network-ID enrollment to the same sorted
registration boundary. Clones authored during play still register immediately
after that boundary. Install the matching corrected core on both peers; an old
client's early-appended IDs cannot be corrected safely by pretending its ordered
fingerprint matches. Dedicated startup also catches up an already-existing
authoritative world once when bridge hooks were installed after InitGameState.

Slow RuneSchema phases (100 ms or more) emit rate-limited
`[PERFORMANCE][SLOW-WORK]` rows, at most once per phase per ten seconds. They
attribute observed work; they do not establish a general EOS/network diagnosis.
Live multiplayer joining, server inventory/teleport and placement still require
acceptance testing against a loaded playable world.

The Authoring Tools workspace remains separate from ordinary settings. Offline
save-cleanup tools export new character JSON copies only; they do not overwrite
an original or bypass world-pruning registry checks.
