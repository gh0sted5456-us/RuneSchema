# RuneSchema 0.8 Live Demo

Copy the `RuneSchema08Demo` folder into:

`RuneSchema/mods/RuneSchema08Demo/`

This demo requires the `RuneSchema-0.8-test-bed` runtime.

On the character-select screen it performs two visible checks:

1. `$RuntimeWidget` forces the existing Edit Appearance button visible/enabled
   and invokes `SetRenderOpacity(1.0)` through the new reflected `$Call` path.
2. `$RuntimeUI` creates a transient top-center panel that reads
   **RuneSchema 0.8 Runtime UI Active**. The TextBlock text is finalized through
   another `$Call` to `SetText`.

If the normal character-select UI appears but the status panel does not, inspect
the RuneSchema log for `$RuntimeUI` diagnostics. If the panel appears, the
transient UMG construction, child/slot assembly, reflected property writes,
viewport attach, and reflected function call path all executed.
