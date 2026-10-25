# 0.8.0.1b — Quick bug fixes

October 24, 2026. Changes since [0.8.0.1](CHANGELOG-0.8.0.1.md):

- Added right-click **Reorder** for confirmed Last delivery items in Helpy's cart; the heading reorders all confirmed items. Actual sent quantities return to the cart for review, never automatic delivery.
- Let readable inventory/storage sections continue cleanup when another container has an unfamiliar format. Unknown data is retained and reported.
- Preserved tracking and location dependencies for unfamiliar quest records while allowing verified orphan quests to be pruned.
- Prevented out-of-range equipment indices from wrapping to unrelated inventory slots during cleanup or baseline recovery.
- Removed repeated whole-save copies from section-by-section cleanup.
- Made cooked registry path deduplication, policy conflicts and Schema provenance checks case-insensitive; failed batches leave prior declarations intact.
- Added cooked building enrollment/menu placement and legacy-PAK JSONC registration examples. Cooked gameplay rules remain in the PAK, with explicit loose placement/unlock overrides supported.
- Added RS-BLD-023 diagnostics for rejected cooked buildings, with Helpy guidance and updated authoring schemas/reference.
- Aligned source, plugin, display and packaging metadata with 0.8.0.1b; Helpy recognizes the bugfix suffix as newer than 0.8.0.1.

Backup and settled-registry safeguards remain in place. Skills/SkillPerks are
still declaration-only, not newly enabled gameplay registries.

This is a source update on main and experimental. The existing 0.8.0.1
release tag and downloads are unchanged; no 0.8.0.1b release asset is published
by this commit. Automated verification is not a live-game/save compatibility guarantee.
