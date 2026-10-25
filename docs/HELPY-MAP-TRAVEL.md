# Helpy map travel

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
`TeleportationSubsystem.TeleportWithParams` service with loading-screen support,
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
