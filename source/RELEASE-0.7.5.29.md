# RuneSchema 0.7.5.29

This revision moves the remaining built-in content trace preset out of the DLL
and makes trace jobs JSON-owned diagnostics.

- The hard-coded character-editor trace preset and its specialized console
  audit were removed from the runtime.
- Building-menu and character-editor filters are supplied as disabled example
  profiles under `settings/jobs`.
- Enabled `RuneSchema.TraceJob.v1` profiles write unique event records to
  `runtime/live/jobs/exports/TraceJob-<id>.json`.
- Individual event lines no longer enter the UE4SS console unless that profile
  explicitly sets `consoleEvents` to `true`.
- Job identifiers, fields, filters, limits, and duplicates are validated before
  the shared ProcessEvent runner is armed.
- The runner is thread-safe, batches result snapshots, caps every job, and
  prevents exceptions from crossing the global ProcessEvent callback boundary.

Startup timing, manual Player/Appearance traces, and optional vendor interaction
exports remain separate opt-in diagnostic tools. They are not auto-running
content trace jobs.

## Processing-station clone safety

- Authored recipes now use deterministic rooted objects under
  `/Game/RuneSchema/Recipes/` instead of `/Engine/Transient`, so a timed queue
  retains a stable recipe identity across its lifetime.
- Processing arrays accept RuneSchema `/assets` clone outputs only after the
  ItemSubsystem registration, stable PersistenceID, and rooted object lifetime
  have all been verified.
- A clone that fails those checks is isolated to that recipe placement. Cooked
  outputs and the rest of the station continue loading normally.
- SafeSave uses PersistenceID as the durable identity. Owner, folder, runtime
  path, and internal-name fields may be refreshed when the same content is
  reorganized without producing a false removal.
- String object references can recover a moved RuneSchema runtime object by a
  unique, type-compatible stable name. Ambiguous and wrong-type matches fail
  closed.
- A recipe with any unresolved property is neither placed nor unlocked, which
  prevents partially initialized ItemData pointers from reaching crafting or
  timed-processing code.

## Helpy

- Incorporated the retained-frame collection UI from the experimental Helpy
  work: warm flat styling, consistent panel geometry, larger viewport scaling,
  and bounded incremental icon loading.

## Loader folders

- Loader directory names are ASCII case-insensitive. For example, `recipes`,
  `Recipes`, and `RECIPES` all select the `/recipes` loader, including nested
  JSON discovery and automatic reload routing.
- `players`, `nameplates`, and known-folder diagnostics follow the same rule.
- A mod containing multiple loader folders that differ only by case is rejected
  for that section so content is never loaded twice in an undefined order.
