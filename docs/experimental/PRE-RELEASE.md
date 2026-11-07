---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

# Pre-release · 0.8.1.0

!!! info "Available for testing · pending promotion to stable"
    Stable RuneSchema remains **0.8.0.3**. The changes below belong to the
    **0.8.1.0 prerelease** and have not been promoted to the stable guides.

<div class="rs-actions">
  <a class="md-button md-button--primary" href="https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0-pre.20261106/RuneSchema-0.8.1.0-complete-20261106.zip">Download complete 0.8.1.0</a>
  <a class="md-button" href="../INSTALLATION/">Installation</a>
</div>

The downloads follow the maintainer's complete package layout. They include
Helpy and all bundled plugins. Install UE4SS
separately; preserve your settings, mods and saves when updating.

## Complete candidate refresh

The complete 2026-11-06 package includes the current core, Helpy, bundled plugins and
matching Lua adapters. It retains the maintainer-assembled package layout; UE4SS
is installed separately. The complete 68-file refresh is published under
[`0.8.1.0-pre.20261106`](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0-pre.20261106).
Earlier tags and downloads are retained for reference. Use the complete package above.

- **Runtime layout:** Core plugins live in `dlls/plugins`; Helpy and its native
  plugins live in `Helpy`. Existing user settings and mod folders are preserved.
- **Controller interaction:** Right-stick pointer control, confirm/context clicks,
  stick-click drag and multi-selection, and clearer controller glyph feedback.
- **Item Bay and mods:** Direct delivery works with a populated cart. Mod cards
  are selectable and expose their bundled licenses. Mod Menu 2 supports up to
  1,024 combined settings, tab keys and template entries.
- **Core startup:** Redirected AppData is resolved to its physical application
  root. Redirected child caches remain rejected, with the exact path reported.
- **Mod-local services:** Optional typed template storage and native numeric
  providers, scoped to their declaring mod and local controller. Native modules
  initialize on the game thread. This API remains experimental; see its
  [contract and verification limits](MOD-LOCAL-SERVICES.md).
- **Cooked interfaces:** World components and mod-owned host console commands
  retain explicit registration and authority checks. Remote coverage is pending.
- **Diagnostics:** Player-mark bridge outages use bounded retry backoff and one
  warning per outage. The shared diagnostic registry includes local-service errors.

## Pending changelog

These changes are available in the candidate and await stable promotion.

| Area | Changes |
| --- | --- |
| [Helpy](HELPY-STANDALONE.md) | Consistent two-pane menus, runtime-bound desktop/overlay presentation and clearer settings. |
| [Controllers](CONTROLLER-NAVIGATION.md) | Xbox, PlayStation and Nintendo layouts, navigation, glyphs and controller shortcuts. |
| [Item Bay](HELPY-STANDALONE.md#cache-updates-and-notifications) | Saved cache indexes for offline browsing, faster filtering, readable names, multi-selection and carts. |
| [Mods and tools](MOD-METADATA-AND-CONTROLS.md) | Sequential cache builds, ID.txt editing, update fingerprints and opt-in UE4SS loader toggles. |
| [Saves](SAVE-RECOVERY.md) | Targeted cleanup, AppData backups/recovery and readable cleanup notifications. World recovery remains opt-in. |
| [Servers](SERVER-CONTROL.md) | Host/client mod comparisons, download links, PIN access, saved authorization and optional automatic authorization. |
| [Diagnostics](NOTIFICATIONS-AND-HOTFIXES.md) | Cleanup notices, resource observations, play-time reminders and hotfix notifications. |
| [Mod interfaces](SHARED-HUD.md) | Shared HUD, mod-owned profiles and presentation routes; safer Lua scheduling and relay teardown. |

[Browse the feature guides](index.md) for controls and defaults.
[Detailed development history](CHANGELOG-0.8.1.0.md) retains the individual changes
and earlier test-stage notes.

## Testing and feedback

All 274 core and 172 Helpy tests passed, along with package integrity, the
compressed core DLL, licenses, versions, matching source and strict combined-site
checks. Windows shipping-game coverage includes redirected AppData, native
provider initialization and cooked template/typed-array roundtrips. Exact hashes and evidence are in
[the release verification files](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0-pre.20261106).

Proton/Wine, every physical controller/driver and full multiplayer restart
behavior still need live coverage. Mod Menu 2 is optional and key-gated;
world recovery is experimental and off by default. Reported leftover Helpy
processes remain under investigation. Use **Exit Helpy** before updating.

[Report a bug on Nexus](https://www.nexusmods.com/runescapedragonwilds/mods/570?tab=bugs)
with the build, platform, reproduction and relevant diagnostic codes.

## Replaced instructions

??? note "Older instructions superseded by this candidate"
    - ~~The complete ZIP is still being assembled.~~ Use the download above.
    - ~~Install the incremental pre-release-binaries bundle.~~ Use the complete ZIP.
    - ~~Helpy runs from a copied AppData executable.~~ It runs from its runtime folder.
    - ~~UE4SS inventory is always read-only.~~ External management is opt-in.
    - ~~Submit the in-app bug form.~~ Report bugs on Nexus.

## Known limits

See [testing and feedback](#testing-and-feedback) above. Earlier changelog entries
are development history, not a guarantee of live acceptance on every platform.

When this candidate is promoted, its guides and changelog move into the stable
version following [documentation promotion](../DOCUMENTATION-CHANNELS.md).
