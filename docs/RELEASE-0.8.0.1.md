# RuneSchema and Helpy 0.8.0.1

Experimental release, refreshed **October 23, 2026**. This is the cumulative
update from 0.8.0.0. The existing tag/prerelease
status is retained. Updating documentation on `main` is not a stable promotion
or a change to that branch's runtime source.

[Download the full ZIP](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.1/RuneSchema-0.8.0.1.zip)
· [Implemented/removed changes, line by line](CHANGELOG-0.8.0.1.md)
· [Installation](INSTALLATION.md)
· [Previous stable release](RELEASE-0.8.0.0.md)

## Highlights

- Refined Helpy's themed window, native right-hand controls, Home, reorderable
  navigation, centered details and direct auto-launch Quick setting.
- Added shared profiles and Favorite/Junk presentation while retiring Scrap,
  deleted-item storage and companion Recovery restoration.
- Consolidated AppData, source-aware caches and game-update-driven mapping reuse.
- Reduced unnecessary observer, startup, icon, hidden-window and DPI work.
- Extended ordinary cooked registration and the bounded general mod-data relay.
- Hardened backup-first, independent-section cleanup and unfinished-quest pruning.
- Consolidated Diagnostics, structured history, code-specific help and reviewed
  bug-report submission through GitHub.

## Exact package

The approved cleaned archive contains **31 files**, one `RuneSchema/` root,
both compressed DLLs, plugin manifests/content, Lua helpers, default settings
and the consolidated legal notice. It excludes UE4SS.dll, custom mods, personal
saves/profiles, generated caches/mappings, debug symbols and loose UI artwork.
Install `RuneSchema/` inside UE4SS `Mods/`, preserving user settings and mods.
Missing managed state is created when needed.

Optional artwork uses existing cache/download/fallback paths. The cleaned ZIP
does not promise full picture/logo presentation offline on a clean first run;
use **Download Initial Assets** for optional artwork. Generated mappings are
validated in platform-specific AppData caches, not bundled in `dlls/mappings`.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| RuneSchema-0.8.0.1.zip | 5,022,231 | `EB91524934383D646D3C73787A2053C06C9118D6211F59170F2DEF2A6291E96A` |
| RuneSchema/dlls/main.dll | 3,250,688 | `4CFB31438CE1B83853C23741063A9223B880DC2B820091689B992EB278F11A3C` |
| RuneSchema/plugins/RuneSchema.Helpy/dll/RuneSchema.Helpy.dll | 1,538,560 | `6A60FF867315C255E263A70F30D588DC30A6E19DCAB825E3E3F76C301022DB4B` |

Current downloads are the full ZIP, `SHA256SUMS.txt` and
`RuneSchema-0.8.0.1-verification.json`. Superseded assets are retired rather than
leaving an old plugin-only package beside the new core. Keep this DLL pair
together. Content-only plugins retain their own 0.8.0.0 manifest versions;
they are not relabelled merely to match the frontend.

## Supplied defaults

- Helpy opens automatically; disable **Open Helpy at startup** in Quick settings
  to keep it closed until F2 on the next launch.
- `advancedLogging` and `[additionalLogging] enabled` are **true in this ZIP**.
  Turn them off for quieter normal use; detailed logging can increase disk work
  and expose paths in local logs. Review logs before sharing them.
- Automatic mapping updates and the Lua LogicMod loader are enabled.
- BPModLoaderMod script patching, automatic mod reload and Quick Launch selection
  are disabled; no character/world name is preselected.
- Save Bypass is not forced on by default; supported relaxed native loading and
  failure-only fallback are separate from pruning and backup verification.
- Legacy Scrap/Recovery settings do not reactivate those retired features.

## Verification and provenance

The two DLL hashes match the locally built candidate that passed **240 native
offline tests and 102 Helpy tests**, seven catalogue tests, direct-link checks,
strict documentation builds and rendered diagnostic heading audits. Both passed
UPX 5.2.1 integrity and decompression comparisons of executable code, other data,
imports/IAT slots and resources. Relocations/resources/icons were retained.
Compressed Helpy passed hidden-start and dedicated-desktop contracts.

The supplied ZIP was audited separately for structure and exact DLL identity.
This is a **local package refresh**, not a GitHub Actions build artifact.
Documentation commits do not publish outstanding runtime working-tree changes,
move the existing tag, or prove the tag's older source reproduces these binaries.
Do not cite a documentation commit as the DLL build commit.

Automated checks are not live gameplay or frame-time measurements. New cooked
families still need their native consumers validated. A custom repair runtime
service, standalone Helpy EXE, replacement `.runesave` format and native
skill-enum extension are not shipped features. No new game/server session or
installation was performed solely to refresh these release assets.

Back up saves and preserve settings/mod lists when upgrading. Cleanup cannot
guarantee recovery of truncated or genuinely corrupt data. UPX can trigger
security-tool heuristics; investigate the exact detection rather than disabling
security software as a generic workaround.
