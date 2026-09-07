# RuneSchema 0.6.1E installation

This build requires the matching UE4SS StackFix1 host. Install `UE4SS.dll` and its matching `UE4SS.pdb` in the UE4SS root, then install RuneSchema's `main.dll` in `Mods/RuneSchema/dlls/`. Keep the game closed while replacing files.

StackFix1 is a host change that reserves Lua stack space during recursive directory enumeration. Its DLL and PDB must remain the exact pair. The PDB is optional for play but required for symbolized crash reports.

The included `example-mods` directory contains opt-in authoring examples. Copy individual example folders into `Mods/RuneSchema/mods/`; do not copy the whole directory as one mod.

The runtime package will be generated only after RuneSchema is rebuilt against StackFix1. The current source package is reviewable independently.
