# Reusable preset scopes

This adds manual diagnostic reads only. It does not change startup, equipment behavior, damage or appearance. Tools must be activated for each session; reading happens only after Run selected preset. Root lookup may load an explicitly named asset. Following references only reads objects already resolved in memory. No native UFunction search is performed by PropertyCaptures.

```json
{
  "Name": "MyObjectScope",
  "PropertyCaptures": [
    {"Root": "Player", "Path": ["PlayerCombatMagicComponent", "*"]}
  ],
  "CaptureLimits": {
    "MaxDepth": 5,
    "MaxEntries": 128,
    "MaxNodes": 8192,
    "MaxSparseSlots": 4096,
    "FollowObjectReferences": true
  }
}
```

Root accepts Player, Controller, Selected, or a full Unreal object path. Selected uses the last inspected actor/object or selected search object, captured as a string when Run is clicked; it is resolved again on the game thread. Inspect crosshair target first to inspect a building piece, then run SelectedObjectScope. Class/struct/function objects remain schema targets; select an instance or supply a class-default-object path for property values.

Path traverses 1–8 reflected property names through strong object references. The final name can be a specific field or * for all properties of that object. Intermediate wildcards, function calls and writes are rejected. PropertyCaptures permits up to 32 targets and can coexist with legacy preset fields.

The reader supports scalar/enum values, names, bounded strings, resolved object references, structs, arrays and maps. References are identities unless FollowObjectReferences is true. Unsupported types, including some text/set/delegate/engine-specific wrappers, report metadataOnly; this is not an unrestricted memory dump. Soft or weak references are only read if currently resolved. Cycles/already-visited objects are linked by path instead of expanding repeatedly.

Limits: MaxDepth 1–10 (default 7; up to 16 with the session deep-capture opt-in); MaxEntries 1–512 per object/container (default 64); MaxNodes 1–16384 shared across all PropertyCaptures (default 2048); MaxSparseSlots 1–16384 (default 4096). Strings cap at 4096 characters. Arrays/maps report count, entries and truncation; maps preserve key/value pairs and scan sparse indices rather than treating Num as the last slot. Oversized/invalid sparse headers are rejected before enumeration. Omissions, truncation, metadata-only fields and errors remain visible in the report. Broad reads can stall the game; limits bound work but do not guarantee crash safety for every native layout.

## Included research presets

- Anima05RuneMaps: exact primary/secondary equipped-ammo map entries. Run first in a world with a staff equipped.
- Anima06RuneData: complete bounded rune data and a known utility cost module.
- Anima07SpellGraph: optional broader reference-following from those maps into rune/spell data. Use after the focused capture; inspect omission markers before requesting larger scopes.
- AppearanceState: cloak/held-item identities, visible meshes, overlays/material lists, native hide-material map and live effect references. Run immediately after entering the world while already wearing the cloak, then after removing/re-equipping it.
- AppearanceMaterials: resolved body/outfit/cape material properties for the same before/after comparison. Empty optional meshes may produce explicit target errors; retain them.
- RocksplosionDamage: the normal/upgraded Blueprint damage-component templates. Cast Rocksplosion once to ensure its Blueprint is loaded, then run. Paths derive from local cooked exports and need live validation.
- SelectedObjectScope: inspect a placed building piece using the crosshair inspector, then run this preset. Repeat on another material/tier if useful. This does not damage the building.

These are research captures, not the completed Anima replacement or appearance/Rocksplosion fixes. Do not repeat broad captures for unsupported values; use the omission/type report to select the next targeted reader or native investigation.
