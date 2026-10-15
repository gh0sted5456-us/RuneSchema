# Helpy 2.0.4 alpha integration

The standalone API-1 desktop plugin lives here. `source/raw` owns the Unreal
game-thread bridge; this UI does not import UE4SS or call Unreal directly.
Build the normal RuneSchema targets to produce a matching core/plugin pair.
The authoritative plugin manifest, branding and default skin are in
`source/plugins/RuneSchema.Helpy`. Preserve users' existing settings on install.

Windows/MSVC standalone validation:

```powershell
cmake -S source/helpy -B build/helpy-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/helpy-tests
ctest --test-dir build/helpy-tests --output-on-failure
```

Tests include real Direct3D renderer fixtures with synthetic data, not a live
game. `source-cache.exe <RuneSchema-folder>` performs a read-only fingerprint
inspection and prints its key, file count and duration. Both copies of the
fingerprint implementation are parity-checked and independently tested.

Source identity uses game-relative paths, extended-length Windows paths and a
Win64/WinGDK lane. Authored content changes and removals still invalidate the
cache; installation aliases do not. Browse cancellation remains monotonic
across initial source inspection and world generation changes. Icon capture is
visible-demand only, with one outstanding capture, bounded queues/retries and
explicit reload recovery.

The 2026-10-14 native-worker crash remains under investigation. These fixes do
not establish its cause or demonstrate a gameplay crash fix. The combined
candidate also catches up already-shown world cells for configured spawns;
native save depletion, transient ownership and cleanup guards remain intact.
Live icon display, silver-seam placement, teleport, save/reload and multiplayer
behavior require separate in-game validation.

Original UI handoff: Helpy 2.0.4 from the RuneSchema UX Adaptation project.
The imported UI follows its component licenses under `licenses` and `vendor`.
