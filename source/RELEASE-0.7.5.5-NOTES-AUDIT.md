# RuneSchema 0.7.5.5 notes acceptance audit

This file maps every item in `RuneSchema Notes.txt` to the 0.7.5.5 source and
release checks. The note is treated as the acceptance checklist, not as code.

## 1. Native time of day

Ground truth was rechecked against the 1.0.0.2 FModel/RSDW export at archive
commit `76c9fd547cc2cc6ebf7773d61d5cacf69a939b12`:

- `BP_InGameTimeActor` derives from `/Script/Dominion.InGameTimeActor` and
  supplies `RealTimeMinutesPerInGameDay`, `InitialTime`, `TimeOfDawn` and
  `TimeOfDusk` (the audited defaults are 24, 11:00, 04:30 and 22:00).
- `BP_Torch_Standing_InGameTime` and `BP_Torch_Wall_InGameTime` set
  `bUsingInGameTime=true` on the native `LightSourceBase` route.
- `BP_Enemy_Campfire_InGameTime` and `BP_NPCAlly_Campfire_InGameTime` use an
  `/Script/Dominion.InGameTimeSensorComponent`, bind
  `OnEnterTimeFrameDynamic_Event` and `OnExitTimeFrameDynamic_Event`, and use
  the audited 05:00-22:00 sensor window.

RuneSchema now observes both the native `InGameTimeActor` phase event (when a
build reflects it) and the sensor enter/exit events used by vanilla fixtures.
Every transition is verified by reading the authoritative live world actor:
replicated phase first, public getters second, reflected clock fields last.
There is no actor/world scan loop. The one-second read remains only as a bounded
fallback for builds that hide all transition functions.

Implemented time consumers:

- `/quests`: acceptance and reward-delivery gates.
- `/spawns` and `/buildings`: authority-owned spawn/despawn lifecycle for AI,
  resources, actors, functional building pieces and fixture props.
- `/events`: spawn and despawn phase gates; weather and actors clean up together.
- `/vendors` and vendor-targeted `/recipes`: category and individual-offer gates.
- `/dialogue`: branch and requirement gates.
- NPC definitions: authority spawn/despawn lifecycle.
- Niagara visual effects: local presentation follows the replicated owner and
  authoritative lifecycle, including phase-gated effects.

`/raw` and `/blueprints` are mutation mechanisms rather than world-lifecycle
owners. They can patch the native data-table time fields and the exported
`InGameTimeSensorComponent` properties directly. RuneSchema intentionally does
not repeatedly rewrite arbitrary CDO/data-table state at dawn and dusk because
that cannot be safely rolled back or replicated. Dynamic behavior belongs in
the typed loaders above.

Grounded `/spawns` and `/events` actors share the same placement contract:
trace the authoritative blocking surface, place the actor 10 cm above it, and
treat an authored `$+offset` or `GroundOffset` as an additional delta. This
avoids floor intersection while preserving intentional author adjustments.

## 2. Vendor and recipe ordering

`Order` is validated on offers and `OrderedWithinCategories` performs stable
ordering inside each category. It is applied to `/vendors` and to the recipe
rows used by a vendor. Categories remain in their authored order.

## 2.5. Player schema

`/players` exposes implemented selectors (all, name, GUID and load slot),
appearance, vitals, stamina, carry capacity, movement, attack, defense,
resistances, named attributes, equipment/effects, archetypes, ghost/visual
rules and reusable or inline nameplates. Inventory, quests and journal data
remain with their owning typed loaders instead of unrestricted save mutation.

## 2.6. Nameplate conditions

`/nameplates` supports built-in combat, spell, fishing, gathering, damage,
respawn and emote activity; skill-XP mappings; GameplayEffect state; and exact
native or Blueprint event paths with parameter comparisons and pulse/activate/
deactivate behavior. This is the generic bridge for quest, event, vendor,
recipe, building and other activity conditions without polling saves.

## 3. Helpy

Helpy uses a fixed dark shell and rail with a consistent three-column,
four-row workspace. Items use twelve fixed icon cards; NPC/AI and Resources use
twelve fixed text cards. Search, filters, pagination, status and actions retain
stable geometry. The plugin DLL carries a plain-data RSDW seed catalogue, so
opening displays the first page immediately and requests only the lightweight
player/authority roster. Opening never starts a UObject census, disk-cache
rebuild or network reference fetch. Refresh validates known/current-world
records; Full Scan remains the only mounted-registry census.

## 4. Universal DLL and build

There is one 0.7.5.5 core DLL. Runtime storefront detection selects Steam/GOG
or Game Pass behavior. Game Pass can consume its runtime's
`UE4SS_Signatures`; Steam uses normal UE4SS/native paths. The build fetches the
pinned UE4SS dependency graph through GitHub HTTPS, builds core and Helpy once,
UPX-compresses and verifies both DLLs, attempts signing without making it a
release gate, and emits one universal RuneSchema ZIP plus the two UE4SS runtime
ZIPs. No package contains a `mods` directory.

Storefront selection and every successfully initialized plugin are standard
startup announcements. Plugin lines use `Version` and `ConsoleMessage` from
each `plugin.json`, including the Networking content plugin and its pak count.
Future plugin version/message changes therefore do not require a core rebuild.

## 5. Registry and client/host presentation

Every mod's `/registry` records are compiled by the registry loader into the
shared bridge, with duplicate identity rejection and deterministic manifests.
Authority validates client actions against that manifest. Replicated world
identity/state drives client reconstruction. NPC pose/emote cues, timed actor
lifecycle and Niagara presentation are applied on clients from the replicated
owner/state rather than attempting to replicate transient local components.

## 6. Buildings

`/buildings` accepts real `BuildingPieceData` placements and fixture-style
mesh/collision props through distinct managed building identities. The runtime
tracks stable `BuildingPropID` ownership, streams placements by cell, publishes
world identity, and applies the protected-deconstruction guard. Removed mod
definitions are not respawned.

## Quest acceptance toast

First acceptance calls the game's native `GiveQuest` with `bSilent=false`,
which is the primary vanilla quest banner in standalone, listen-server and
dedicated-server authority. After dialogue releases the HUD, RuneSchema sends
an owning-client `Client_DisplayTextNotification` fallback and the whole-registry
`Client_OnQuestsUpdated` refresh. The fallback retries up to three times after
the first attempt if the client/HUD contract is temporarily unavailable; it is
never broadcast to unrelated players.

## Release gates

The builder compiles and runs focused contracts for vendor ordering, schemas,
NPC/time catalogs, player activity/nameplates, native quest ownership and
notification, events/dialogue, buildings/resources, Niagara and the FModel-
derived time transition paths. The final package also passes the UE4SS import/
export ABI audit and UPX verification.
