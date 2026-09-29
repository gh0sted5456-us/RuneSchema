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

Planned next:

1. Existing dot-separated owner paths remain the preferred target mechanism.
2. Add scoped discovery for the owner's WidgetTree.
3. Add HUD discovery through known live HUD references.
4. Add CommonUI container/activatable discovery with CDO and stale-instance filtering.
5. Do not expose unrestricted `FindAllOf(UserWidget)` as a general authoring primitive.

The RSDW tooling showed why this boundary matters: CDOs, pooled widgets, and
partially constructed Slate trees can be present in global scans.

## Phase 3 — RuneSchema-owned transient UMG

Planned after discovery is stable:

- `$RuntimeUI` owns a transient UUserWidget + WidgetTree.
- Small supported primitive set first: CanvasPanel, Border, TextBlock, Image,
  Button, and common slots.
- Explicit ownership, viewport Z order, teardown, and duplicate prevention.
- No attempt to synthesize arbitrary Blueprint bytecode.

The goal is to cover overlays, notifications, lightweight controls, and similar
UI without requiring Lua or a cooked WBP while keeping full Blueprint creation
outside RuneSchema's runtime schema contract.

## Test-bed CI

Pushes to this branch run the isolated `runtime-widget-v08-contract` on Windows.
The lane intentionally avoids the full UE4SS bootstrap because the pinned UE4SS
commit currently references the retired `Re-UE4SS/UEPseudo` submodule; that
external dependency failure is unrelated to the 0.8 runtime-widget contract.
