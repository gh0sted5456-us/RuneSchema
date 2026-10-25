# 0.8.0.2 — Cache compatibility and current DLLs

October 25, 2026. Changes since the published [0.8.0.1 package](RELEASE-0.8.0.1.md).
Both RuneSchema and Helpy are rebuilt as 0.8.0.2; this is a binary release.

## RuneSchema

- Includes the [0.8.0.1b fixes](CHANGELOG-0.8.0.1b.md): cooked building enrollment, case-insensitive registry matching, legacy-PAK JSONC registration examples, and RS-BLD-023 guidance.
- Keeps readable save sections eligible for cleanup when another section has an unfamiliar format; unknown data remains retained and reported.
- Preserves unfamiliar quest tracking/location dependencies while pruning verified orphans.
- Prevents equipment-index wraparound and removes repeated whole-save copies during independent-section cleanup.

## Helpy and caches

- Adds right-click **Reorder** to confirmed Last delivery entries; quantities return to the cart for review.
- Reads the current data-only GitHub cache catalogue independently of the DLL release tag. Optional UI artwork keeps its separate immutable manifest.
- Prefers the matching game build and locale without refusing all other native cache candidates.
- Reuses native pictures across changed cooks only at proven native paths, marks them display-only, and leaves current item IDs to live discovery.
- Adds owner-scoped fingerprints so a mod's own bundle remains compatible when a neighbouring mod shares its mounted namespace.
- Retains completed downloads while live source verification, another artwork task, or disabled disk caching prevents import from starting.
- Supports long cache paths for atomic download staging, verification, reuse and cleanup.
- Makes the About service use the same version as the plugin descriptor.
- Publishes a verified Steam hotfix cache with **2,903 item/journal records and 4,246 pictures**, with explicitly partial coverage.

## Author tools and documentation

- Adds the optional [single-file Bash cache builder](modders/offline-helpy-cache.md): place it beside a staged mod's PAK trio/banner and generate its `cache/` while that game installation is closed.
- Adds a native-only mode and optional separately partitioned mod outputs. No upload, game launch, PAK editing or save mutation runs automatically.
- Adds locked dependency restoration, source/script parity checks, image/fingerprint self-tests and a dedicated Windows CI check for the author tool.
- Updates the guides with worked FModel-export, creature-drop, cooked-item, `$Clone`, `$Append` and targeted-patching examples.
- Aligns the current-download page, tag, source versions, Windows DLL metadata and plugin manifests with **0.8.0.2**.

The retired Scrap/inventory-deletion workflow, Recovery restoration mode,
replacement save format and standalone Helpy EXE remain absent. Skill/SkillPerk
declarations do not become new native gameplay registries in this release.
See [package and verification](RELEASE-0.8.0.2.md).
