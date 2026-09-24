# RuneSchema 0.7.5.16

This release continues the runtime character-customization and standalone-building audit.

- Adds live character-menu boundary diagnostics. At selector construction RuneSchema reports the HairPreset CDO count, HairPreset DataTable row count, and relevant live selector arrays.
- Normalizes readable character compatibility values in `/assets` append operations so character options use the same contract as registry/DataTable patches.
- Retains the 0.7.5.15 direct `BuildingPieceData` lifetime, world re-resolution, catalogue replay, vendor-category refresh, diagnostic jobs, optional mod identity metadata, and spawn-limit corrections.
- Ships the same Universal and Core package structure as 0.7.5.15.

The new character diagnostics intentionally distinguish successful data mutation from successful native UI materialization. The building audit likewise treats native widget materialization as a separate requirement from catalogue registration.
