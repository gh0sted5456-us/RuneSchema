# RuneSchema 0.8 test bed

Branch: `RuneSchema-0.8-test-bed`

This branch is the isolated proving ground for the next Blueprint/runtime
authoring layer. Stable 0.7.x behavior remains the compatibility baseline.

## Phase 1 — runtime actions

Implemented in the first 0.8 test-bed slice:

- `$RuntimeWidget.$Call`: invoke an existing reflected UFunction with named JSON arguments.
- `$RuntimeWidget.$When`: gate a rule on one or more observed ProcessEvent function names.
- `$RuntimeWidget.$Once`: run a live-owner rule once per world lifetime.
- `$RuntimeWidget.$Activate`: CommonUI-aware ActivateWidget / DeactivateWidget convenience.
- Per-rule ProcessEvent re-entrancy suppression.
- World-teardown cleanup for runtime execution state.

The implementation reuses `ActorHelper::FunctionCall` so reflected values are
initialized and destroyed using the same parameter lifecycle already used by
RuneSchema native runtime systems.

## Phase 2 — safe discovery

Implemented:

1. Existing dot-separated owner paths remain the preferred target mechanism.
2. `$Find.Scope = "WidgetTree"` is an exact-name fallback below the owner's live tree.
3. `$Find.Scope = "HUD"` walks only live `HUDWidgetRefs` for the owner's world.
4. `$Find.Scope = "CommonUI"` walks only CommonUI container `WidgetList` arrays.
5. CDOs, archetypes, loading/destroying objects, oversized source arrays, and ambiguous matches fail closed.
6. Discovered targets are weak-tracked back to their live owner for later ProcessEvent refreshes.
7. There is no unrestricted `FindAllOf(UserWidget)` authoring primitive.

The RSDW tooling showed why this boundary matters: CDOs, pooled widgets, and
partially constructed Slate trees can be present in global scans.

## Phase 3 — RuneSchema-owned transient UMG

Implemented first slice:

- `$RuntimeUI` owns a transient UUserWidget + WidgetTree.
- Explicit primitive whitelist: CanvasPanel, Border, TextBlock, Image, Button.
- Recursive child construction with reflected panel-slot properties.
- Existing `$Bind` and test-bed `$Call` can operate on created nodes.
- 64-node / depth-8 / 32-children structural limits and exact safe names.
- Explicit viewport Z order, weak ownership, duplicate prevention, and teardown.
- No attempt to synthesize arbitrary Blueprint bytecode.

This covers overlays, notifications, lightweight controls, and similar UI
without requiring Lua or a cooked WBP while keeping full Blueprint creation
outside RuneSchema's runtime schema contract.

## Test-bed CI

Pushes to this branch run the isolated `runtime-widget-v08-contract` on Windows.
The lane intentionally avoids the full UE4SS bootstrap because the pinned UE4SS
commit currently references the retired `Re-UE4SS/UEPseudo` submodule; that
external dependency failure is unrelated to the 0.8 runtime-widget contract.

## UE4SS source bootstrap

The 0.8 builder now prepares the pinned UE4SS checkout before CMake configure.
UE4SS's `UEPseudo` submodule is private, so local source builders must connect
their GitHub account to Epic Games, accept the Epic organization invitation,
and authenticate Git HTTPS. The builder rewrites UE4SS's SSH submodule URLs to
HTTPS, validates UEPseudo access up front, initializes the pinned submodules, and
passes the prepared tree through `FETCHCONTENT_SOURCE_DIR_UE4SS`.

This keeps private-repository authentication out of CMake's nested FetchContent
step and gives a direct setup error when the account has not been authorized.
The isolated 0.8 contract workflow remains intentionally independent of the
private UEPseudo repository.
