# Helpy map travel

In the next maintenance build, map action panels open in the center of Helpy. Their headings, coordinates
and mode controls are centered; search fields and player selectors fill the
available form width. Long content scrolls inside the panel, with the action
button retained below it. Dialogs and floating tools open centered and can
still be moved; hover details stay centered on the control that owns them and
context menus remain beside the clicked target.

In **Helpy → World → Map**, click a destination, choose **Teleport**, select the player,
then choose **Teleport player here**. This extends Helpy only, not the game's
normal map. Use the host's Helpy in multiplayer; remote clients do not receive a
new unauthenticated teleport command.

The destination need not already be visible or have collision loaded. A transient,
non-replicated streaming-source actor prepares the clicked area while the player
stays at the departure point. The game thread polls activated-cell readiness
without sleeping or forcing a synchronous world-streaming flush. It then checks
the landing footprint against blocking ground over the native map's full height
range. Unavailable ground, cliffs and invalid capsules prevent departure.

After preparation, RuneSchema invokes the game's reflected
`TeleportationSubsystem.TeleportWithParams` service without a second loading overlay,
normal checks enabled (`bNoCheck=false`, `bIsATest=false`), and the verified target.
This uses native travel rather than a raw `K2_TeleportTo` position jump or a fake
permanent lodestone. No named lodestone, unlock, cost item or world-save record is
created. Arrival requires both a nearby verified target position and the native
player's `IsTeleporting` state to finish; acceptance alone is not success.

Preparation has a 30-second bound; accepted native travel gets a further 60 seconds
to confirm arrival. The mutation mailbox stays occupied for the whole operation,
preventing duplicate requests. A world/generation or possession change invalidates
the job. Cancellation before departure leaves the player unmoved; after native
acceptance RuneSchema does not issue another teleport to undo an in-flight journey.
An unconfirmed arrival is reported as such, not retried automatically. Only the
owned temporary streaming helper is retired. Spawn and Build keep their existing
loaded-ground requirements.

Terrain discovery advances past one overhead obstruction per preparation slice.
Footprint checks retain completed samples, perform at most two new surface
queries per slice, and yield between queries after a cooperative 2 ms budget.
Preparation slices can resume after 20 ms; rejected candidates retain a 100 ms
backoff. A single native physics call cannot be interrupted by that budget.
The measured centre floor guides the remaining footprint rays, so each edge
does not repeat full-height terrain discovery. Streaming readiness changes
discard the partial footprint. Invalid collision contracts stop the operation
instead of retrying an unsupported call for the entire preparation timeout.

`[HELPY][MAP-TRAVEL]` phase, elapsed-time and final-result lines are written to
both UE4SS and RuneSchema logs without requiring advanced logging. Include them
when reporting a stalled journey. A native travel timeout remains unconfirmed;
RuneSchema does not clear the game's teleport state or pretend arrival succeeded.

The map displays a compact animated activity bar, current phase (Preparing,
Checking ground, Travelling) and elapsed seconds. It deliberately does not show a
percentage: native streaming exposes readiness, not a reliable completion fraction.
Cancel travel is available before departure; native travel must finish after it
has committed. Progress messages do not complete the action or release its slot.

Teleport selection skips the placement-only ground preview and building-catalog
lookup, including repeated clicks on lodestones. The travel job still performs
its own activated-cell and capsule-footprint checks before moving the player.
An already-running runtime request must finish before another mutation can start.

## Wide map and ground height

The map fills the available rectangular workspace while preserving native map
proportions. Scroll or use the zoom slider up to 32×; wheel zoom stays anchored
under the pointer. Drag to pan; Fit world shows the full map, letterboxing when
necessary. Clicking outside the artwork cannot select an out-of-bounds target.
Magnification uses the native artwork, not newly generated terrain imagery.

Pause the pointer to read X/Y and native ground Z. The selected teleport popup
also requests Z independently, without making it a prerequisite for travel.
Requests are debounced and coalesced into one read-only latest-point mailbox;
the game thread performs the same collision-ground query used for map placement.
These queries cannot reserve/release the mutation slot and yield to active travel
or queued actions. They do not load distant cells. No loaded ground hit means
ground unavailable, not Z=0 or an inferred height. Replies are correlated by
request and world generation; moving the pointer invalidates the displayed Z,
and stationary height samples expire after two seconds before refreshing.

## Compatibility and validation

The implementation uses live reflected parameter layouts and semantic enum names,
not executable offsets or storefront-specific byte patterns. Missing or changed
native functions stop this feature safely. The current native API was checked
against local generated headers and the current mapping; builds and deterministic
tests do **not** establish in-game success.

Before release, test Steam and Game Pass in a disposable world: loaded nearby point,
distant never-visited point, high/low terrain, roof/bridge, cliff/water rejection,
travel already in progress, world exit while preparing, then host-selected remote
player on a dedicated server. Confirm client loading-screen behavior and that no
streaming-helper actor persists after saving/reloading. Map bounds do not mean
every pixel is walkable: unavailable/unsafe destinations must still fail.

The temporary-source preparation pattern follows Epic's
[World Partition documentation](https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine).
