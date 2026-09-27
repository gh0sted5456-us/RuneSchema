# RuneSchema 0.7.6.6

## Stability

- Fixed a Steam and Game Pass crash when leaving a world and entering another one. Player scale state now uses a serial-validated weak reference and is discarded when the active world changes.
- Registry networking uses one process-lifetime deferred callback instead of removing and registering callbacks during world travel.
- Front-end and menu worlds cannot receive a gameplay registry bridge.
- Optional features continue to degrade independently when a native binding is unavailable.

## Save cleanup

- Removed the player-content ledger, declaration tag, restore history, and retired-content bookkeeping.
- Enabled mods register their live identities first. RuneSchema then removes unresolved RuneSchema-supported identities before character hydration and lets Dragonwilds save normally.
- Steam/GOG and Game Pass use the same cleanup decision through their storefront-specific save lanes. Xbox Game Save containers are not edited directly.
- Reinstalling a removed mod is treated as a fresh installation. Deleted mod state is not restored.
- Building state relies on the native world save and stable building identities; RuneSchema no longer keeps a separate building manifest.

## Loaders and schemas

- Building clones and overrides support isolated placement, stability, derived placement, shelter, health, snapping, and processing-fuel settings.
- Building profile changes copy native rows instead of mutating a shared vanilla row.
- Recipe item references remain path-first, with persistence identity validation performed by RuneSchema where required.
- Loader schemas and examples no longer advertise the removed declaration and ledger metadata.
- Nested loader folders remain supported without case-sensitive folder-name requirements.

## Diagnostics and verification

- Normal output remains limited to useful loader summaries, warnings, and failures.
- Added lifecycle regression coverage for registry callbacks and player scale state across world travel.
- The release build passes all 43 contract tests.

## Packages

- **Universal** contains RuneSchema and the optional bundled plugins.
- **Core** contains RuneSchema without optional plugins.
- Both packages use the same `main.dll` for Steam/GOG and Game Pass; RuneSchema selects the correct lane at runtime.
