# Implementation constraints

This document holds explanations that previously lived in source comments. Keep only a short local comment where a programmer could otherwise miss an ownership, ABI, ordering or data invariant.

## Loading and ownership

The main loader stops and joins its file watcher before unregistering callbacks or destroying loaders. The watcher queues file paths; engine work runs on the game thread. Loaders are destroyed while their table registry is still alive. The registry permits recursive loads on one thread, and callback snapshots run outside their collection lock so callbacks can add or remove registrations.

Inline detours publish their trampoline before enabling the hook. Blueprint callbacks cannot write until both required hooks are ready. Surge and Shadowveil verify all executable sites before patching any site. An exception must not unwind into the native game caller. The native contracts' site labels and version bytes remain beside the code.

Player appearance, spawn work and player joins share the existing world/runtime handlers. Spawn modifiers share the blueprint loader's PostInitializeComponents observer. Creating another loader or competing detour for the same event risks conflicting ownership. The existing phase and AA_/normal/ZZ_ order are unchanged by the documentation pass.

## Engine memory and reflection

Unreal objects may already be unavailable during DLL static destruction. EngineCleanupLifetime disables engine access before owning static containers destruct; live-world cleanup still releases roots. In PlayerGhost, the stop barrier must remain declared after the owning containers. A background file listener must outlive its watcher threads.

Map allocation uses the padded map/set layout, including value alignment. Adding key and value sizes can underallocate. Sparse container occupancy is independent of its live count; iteration bounds must use occupied slot ranges. Diagnostic traversal caps emitted values and sparse slot visits separately.

Reflected array edits stage deep copies, including nested arrays and strings. Reference selectors operate on the reference itself and do not walk into a shared UObject while staging. Soft object paths preserve the full suffix after ':'; string views are copied with their explicit bounds rather than assuming a NUL terminator.

## Appearance and equipment

Item $VisualEffect definitions are metadata keyed by item identity, not native UObject fields. The shared player material owner combines item and player rules. Specific equipment rules override whole-person rules on the corresponding mesh. It records original materials and restores only slots that still contain its own material, so another writer's replacement is not erased.

Dynamic materials shared across streamed actors use GameInstance ownership. A tree can unload while other trees still use the style. Physical mesh filtering excludes WidgetComponent, which otherwise turns nameplate quads into visible ghost rectangles. Resource actors expose meshes through different fields, so component enumeration covers their physical models. Overlay-only remains the legacy default; body material replacement is opt-in.

Wearable clones retain the source stats table but use a row named after their own InternalName. /raw can therefore supply distinct stats without repeating a DataTableRowHandle in /assets. Shadowveil action protection matches the effect's originating item because equipment slot replication can lag the equip notification. Native Surge reads the current equipped legs through the existing evade component; it does not require a polling loop.

## Player data and spawning

A character GUID takes precedence over a display name. Existing native save values should not be rewritten solely to repeat an appearance choice. When a mod first changes appearance, the observed saved value takes precedence over a generic authored fallback. Preserve that first observed fallback across overrides; an explicitly supplied fallback is used when the prior value was never observed. Disconnected characters are reconciled when they reconnect.

Health updates select an authoritative live component with a positive maximum, rather than a template component. Use the public player attribute pool and reflected health API; invoking private health attribute delegates can terminate the game. ModifyMaxHealth expects an absolute maximum. Maximum health is derived from the live MaxHealthAttribute, so notifications must follow a change. GE_ModifyMaxHealth uses the game's normal permanent-health path. If the high-level wrapper rejects a call during early pawn initialization, the component fallback preserves Dominion's native 24-byte effect handle. Vitals remain outside generic attribute writes because they require dedicated notifications.

Blueprint AI spawn points perform game-side construction and director registration that a bare native spawn point may lack. Some spawn points emit AI before propagating the authored Guid into SpawnInfo; matching occurs for that emitted instance through lifecycle events. Already introduced resource nodes retain their save/respawn depletion interval rather than being refilled on reload. Course orbs use AgilityCourseComponent; wall and arrow blueprints use AgilityCourse.

## Diagnostics

Diagnostics are manual. Native trace callbacks record bounded address tokens without dereferencing them, allocating JSON or invoking engine APIs. Reflected captures run on the game thread and follow only resolved references within their traversal budget. Type targets are inspected as schemas rather than object instances. The equipment/spell API exporter describes native layouts rather than serializing save or inventory state.

Stop a recorder before reflection or file export can fail. Keep an available result in the UI if disk export fails. The Tools guide documents coverage and limits. Regression-test comments identify fixtures and expectations; they are not release history.

## UE4SS readiness gate

The early DataTable detour records addresses only until UE4SS has called on_unreal_init. It performs no table reflection, property edits or class-cache setup before that signal. The queue deduplicates addresses in arrival order and is capped at 4,096 entries. Overflow stops RuneSchema loader initialization with an error rather than silently losing edits.

on_unreal_init publishes readiness and installs the existing GameInstance hook. Core work starts from the next game-thread table event, or the GameInstance callback if no such event occurs. It does not run core loader work on the UE4SS initialization thread. IsInGameThreadRaw is checked only after the readiness signal. Existing post-initialization table processing retains its serialization callback behavior.

Once core setup finishes, a single live-object validation walk matches queued addresses without dereferencing the early addresses. Matching DataTables receive an object-array index and serial snapshot; replay checks the array slot, address, serial and live flags before registering each surviving table. Zero serial is accepted: this UE4SS weak-pointer implementation copies rather than allocates serials, and its Get rejects zero, silently dropping otherwise live tables. The snapshot is temporary and used only during this game-thread replay, not as a persistent weak handle. Reentrant events during core setup are included in the queue. Removed objects are omitted and the replay count is logged. No queued objects are rooted before readiness. The temporary lookup and queue storage are released after replay. This one-time validation walk is necessary because weak object handles cannot safely be constructed before the object system is ready; there is no periodic scan or new tick hook.

The existing signature bootstrap, Dragonwilds version/FName container compatibility setup, and early pak-folder interception remain unchanged. This candidate gates core loader/reflection work; it does not remove every pre-readiness compatibility operation. Loader order, raw-table callback order, Blueprint patch behavior, and installed gameplay JSON are unchanged from DiagDepth1. This is not the rejected PatchOrder1 design.

Initialization exceptions and capacity failures are terminal for this run and are logged. Successful compilation does not establish that boot pauses or heap errors are fixed. Validate startup markers, expected mod rows, clone recipes, world entry and exit.

## Compact2 efficiency

The item-subsystem pointer is local to a synchronous TryApplyPending batch and passed to clone creation and registration; it is not retained across frames/worlds. A successful initial journal pass suppresses repeated calls for other journal folders at the same lifecycle phase. Errors permit retries; explicit auto-reload is unchanged. No new hooks, polling, native-address cache or reordered initialization.

Advanced verbose logging uses enableDebugLogging, false by default. Clone counts distinguish constructed and existing runtime clones; property/registration errors appear separately. Raw patch counts report successful existing-row edits. Blueprint counts report rules registered, not live object changes. StartupTrace Begin stores path/time only; Mark opens/rotates/flushes files only when verbose is enabled. Disabled tracing leaves old files untouched, so their timestamps must be checked.

Spawn loading emits one normal summary containing new, altered, error, AI, boss, resource-node, other-actor, and removal counts. Equipment emits one normal Shadowveil/Surge summary. Successful native-binding and per-entry creation or reconciliation messages are verbose diagnostics; warnings and errors remain normal output.

## Pak folder validation

The native GetPakFolders hook still registers the mods root; Unreal performs recursive discovery. RuneSchema skips known JSON folders, players and canonical paks during unknown-folder checks. Only unknown legacy folders need a compatibility scan; it stops at the first regular pak/IoStore/signature file and reports filesystem errors. Directory names ending in container extensions do not count as content. This helper does not mount files, follow directory symlinks or establish pak priority. The obsolete appearance-folder exemption was removed because no loader reads it. Actual appearance fields remain supported. Loader registration now transfers seven temporary owners directly; stored building/spawn/string references and registration order are preserved.

## LoadTight1

Asset queue compaction and bounded early-table discovery are described in [Loading efficiency](LOADING.md). They preserve queue order, deferred patches, replay-time identity checks and lifecycle dispatch. Neither change alters native object allocation or recipe placement policy.
