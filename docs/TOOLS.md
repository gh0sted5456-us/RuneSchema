# Manual authoring tools

Tools are disabled each launch. Activate them from the RuneSchema tab in UE4SS. Searches, captures and traces run only on request. Heavy diagnostics can stall the game or increase memory use; a restart may be needed afterward.

Tools has Inspector, Presets, Traces, Results and Exports tabs. Each has its own scrolling content; navigation and Save settings remain outside that content. General, Loaders, Logging, Spawns and Load order also scroll independently. Loader and notification switches use separate IDs.

Inspector captures the camera's first blocking target or local player and searches loaded object names. A selected result is highlighted and its path is displayed. The search stops at 250 matches. Captures are snapshots, not continuous polling.

Presets contains the saved-preset selector, Reload presets, Run selected preset and JSON editor. Save uses the preset's Name; saving that Name again replaces that saved preset. See PRESETS.md for fields and bounds. Running creates a timestamped diagnostics report.

Traces contains Player Trace and Appearance event trace. These collect observations only. See TRACING.md for limitations.

Results holds the last snapshot, field filter, copy and JSON export controls. Large JSON is formatted once per changed snapshot. Its scroll region can reach the last field without a fixed-height inner snapshot panel.

Exports contains schema generation and the native equipment/spell API export. Enter a world before exports that need live tables or player data.

Appearance is authored in mod JSON. There is no ghost preview toggle or session scope override. Tools activation does not enable gameplay appearance.

AA_ mod folders run first and ZZ_ folders last, case-insensitively, with stable order within each group. Use ZZ_ for patch mods. Config keys and defaults remain compatible. Restart for a complete load-order replay, adding appearance rules or changing blueprint rules.

## Diagnostic capture depth

Unspecified preset depth defaults to 7. Explicit MaxDepth values from 1 through 10 are accepted normally. Tools > Presets has an opt-in session checkbox allowing up to 16; changing it reloads the preset list. It resets when the DLL starts again. Existing explicit preset depths are preserved. Load, save, and execution all validate the current depth policy.

The extension changes only depth. Defaults remain 2,048 nodes, 64 entries per container, and 4,096 sparse slots; hard limits remain 16,384 nodes/sparse slots and 512 entries. Following object references remains opt-in, with cycle detection and no asset loads during traversal. Deeper captures can still stall the game; these bounds are not a wall-clock guarantee. Diagnostics are manually activated.

Skipped preset details list filenames and actual validation errors (up to 64 displayed errors). The 64-preset capacity is reported separately when reached.
