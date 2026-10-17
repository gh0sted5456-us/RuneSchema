# RuneSchema and RSMK 0.8.0.0

Published from `main`, tag `0.8.0.0`. This release consolidates the October
13–16 alpha development into the verified r7 runtime and the cleaned package.

[Download the full ZIP](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.0/RuneSchema-0.8.0.0.zip)
· [Full consolidated changelog](ALPHA-CHANGELOG.md)
· [Installation](INSTALLATION.md)

## Included improvements

- Implemented the revamped Helpy desktop workspace, themeable branding and Info panel.
- Implemented the detachable/resizable Item Bay, item details, source filters and related-item families.
- Implemented alphabetical catalogue caching, bounded requests and expired-catalogue recovery.
- Implemented multi-recipient carts, removable recipients and stale-world roster fences.
- Implemented source-aware icon caches and import/export/capture workflows with progress reporting.
- Implemented a dedicated Authoring Tools workspace for schema drafts, profiles, traces and diagnostics.
- Implemented editable RuneSchema INI settings with legacy JSONC migration support.
- Implemented RuneSchema.log and a bounded Live Log viewer with filtering, search, local export and linked diagnostic help codes.
- Implemented server-local review/staging for five documented INI settings and a backed-up, stopped-server apply helper.
- Implemented update/build information, UE4SS build identification and runtime compatibility checks without a fixed future build-number ceiling.
- Fixed duplicate network-identity reservations and removed unrelated catalogue count limits.
- Extended cooked and schema registry integration, including recipes, journal/lore and wearable contracts.
- Implemented native recipe discovery overrides and indexed/batched recipe processing.
- Improved journal category/detail routing and suppressed repeated identical unlock warnings.
- Implemented shared quest-scroll contracts and additional quest/event authoring capabilities.
- Hardened streamed spawning, grounded placement, nearby-safe placement and resource-scale restoration.
- Improved map travel feedback, ground-height use and building catalogue/placement validation.
- Hardened quest owner-marker migration and orphan-only cleanup boundaries.
- Bounded startup/catalogue/NPC work and moved detailed performance output into advanced logging.
- Retired the custom UE4SS-host/native nested-addon experiment; complete nested Lua packages remain supported.
- Preserved the unmodified UE4SS.dll boundary on Steam/Epic and Game Pass.
- Cleaned the install ZIP while retaining DLLs, mapping, Lua runtime, plugin assets/manifests, settings and consolidated legal notices.
- Updated the website, authoring guidance, server reference and diagnostic help pages.

## Verification and package identity

Release preparation passed **199 core tests and 45 Helpy tests**, plus release
metadata and license-packaging regression checks. The existing candidate also
passed UPX integrity/round-trip checks, extracted desktop lifecycle checks and
269 core import resolutions against each unchanged stock client host. Server
INI application was tested against disposable fixtures, not a production server.

The ZIP is a packaging-only cleanup of the installed r7 build. Neither DLL was
rebuilt or changed for publication:

| File | SHA-256 |
| --- | --- |
| Full ZIP | `AA568E6EF200C0E6C07615CE676D7D0A7D1A7CE4B6183A00BFF9D0B6219F52EF` |
| Core main.dll | `AECD66692882CDA88596374B7FBFA316B7931588A2E3A32DB52155432B3F9508` |
| RSMK plugin DLL | `8AF9854D0CA5691E7CF71DC2F30752252C27955CE9FE73FC21030BB7F8A18FF2` |

## Important limits

Publication does not close the outstanding live-world tests listed in the
[consolidated changelog](ALPHA-CHANGELOG.md#verification-and-remaining-work).
These include streamed/permanent building placement, mod/clone icon coverage,
quest-scroll/reconnect progression and dedicated/co-op behavior. Admin role
badges are not grant/revoke/kick controls. Headless-server UI requires an
interactive desktop; server-setting changes are staged for offline application.
There is no promise of compatibility with every future game/UE4SS build or a
universal fix for the reported content-related crashes.

Back up saves and preserve authored mods/settings when updating. This release
does not ship or replace UE4SS.dll, and no Nexus files were uploaded as part of
this GitHub publication.
