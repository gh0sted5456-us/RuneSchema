# RuneSchema 0.7.8.0-alpha candidate — 2026-10-15

This source snapshot is for `runeschema-7.8-alpha`. It does not change the main or
experimental release channels or create a release tag.

## Included changes

- INI settings with legacy JSONC migration, dedicated-desktop Helpy startup and
  environment branding; retain installed user configuration and mod enablement.
- Cooked recipe, journal and lore declarations with registry-enrollment health
  checks that veto orphan cleanup when required enrollment fails.
- Bounded conventional-root PAK registry shards, independent of Asset Registry
  enumeration, with atomic declaration publication and cross-owner/path checks.
- Native recipe discovery policies and overrides for supported item, interaction
  and skill prerequisites; native learned-state and unrelated grants are retained.
- Helpy Map destination preparation using a temporary World Partition streaming
  source, verified landing ground and the native teleport service/loading screen.
- Helpy recipe-station identification using typed native station bindings,
  building names/icons and exact recipe membership beyond the generic inspector's
  64-entry limit. Item/upgrade tables and vendor rows are not physical stations.

See [registry shards](COOKED-REGISTRY-SHARDS.md),
[recipe discovery](RECIPE-DISCOVERY.md), [map travel](HELPY-MAP-TRAVEL.md),
[recipe stations](HELPY-RECIPE-STATIONS.md) and [INI settings](SETTINGS-INI.md).

## Verified local package

Filename: `RuneSchema-0.7.8.0-alpha-shards-map-stations-final-20261015.zip`.
This records the locally built candidate; the ZIP is not committed to Git and
this branch push does not publish a tagged release asset.

| Artifact | SHA-256 |
| --- | --- |
| ZIP | `EDE6CC3D7AEDD1D14C0C39F54125E4AB541DB8830F817048084064E614288C65` |
| Compressed RuneSchema DLL | `3C4DAE48E559DAEBE731A8344C6B6A9A89ED45F019F14FF42FADDE32DED696CC` |
| Compressed Helpy DLL | `79822F4F2DEF8A13329291AAF52B065D9098F1EE46654E741E6AA22087EDC725` |

Both DLLs use the normal `UPX -9 --best --lzma` packaging path and passed UPX
integrity checks. All 269 core UE4SS imports resolve against both installed Steam
and Game Pass hosts. Extracted compressed Helpy hidden-window and server-desktop
lifecycle checks passed, along with 168 core tests, 25 Helpy tests and 32 schema
cases.

An independent audit using the candidate's actual shard/manifest/discovery
headers validated Colors' 80 exact declaration payloads across five roots:
21,982 recipe policies and 41,548 prerequisite references, without Asset Registry
enumeration. This checks payload compatibility, not mounted in-game enrollment.

## Still requires in-game validation

- Station textures and complete station coverage on Steam and Game Pass.
- Distant destination streaming, native travel completion, safe rejection and
  multiplayer client loading-screen behavior.
- Native discovery behavior for fresh and existing characters with the new cooked
  content; no completed test here claims universal learned-recipe backfill.

The separately reported NPC `WaitingForCell` / zero-scan problem is not claimed
fixed by this candidate. The separate UX Authoring handoff remains a distinct,
not-yet-applied change set and is not included by this snapshot.
