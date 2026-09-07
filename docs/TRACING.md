# Manual traces

Activate Tools and open Traces. Appearance is configured in JSON; see APPEARANCE.md.

Player Trace records reflected ProcessEvent completion for the starting player/controller and their Outer-owned objects on the starting game thread. Category names are filters inferred from function names. Direct native calls, external world actors and separately owned held actors may be absent. Parameters and call nesting are not recorded. Recording lasts at most 60 seconds and 4096 events; stop/export writes the result. Add action markers to label tests. Tick suppression is enabled by default.

Appearance trace observes five version-checked native appearance sites. It records at most 256 events over 60 seconds, with before/after captures on manual start/stop. Stopping detaches the recorder; the shared hooks remain only if authored player/equipment appearances still need them. It does not change materials.

Both tools remain manual. Game restart may be needed after broad diagnostics. Source tests validate options, shared hook ownership and shutdown policy; only a live game can validate capture coverage on a particular build.
