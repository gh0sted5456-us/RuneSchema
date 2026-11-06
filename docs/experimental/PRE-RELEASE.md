---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

# Pre-release · 0.8.1.0

!!! info "Available for testing · pending promotion to stable"
    Stable RuneSchema remains **0.8.0.3**. The changes below belong to the
    **0.8.1.0 prerelease** and have not been promoted to the stable guides.

<div class="rs-actions">
  <a class="md-button md-button--primary" href="https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0/RuneSchema-0.8.1.0.zip">Download 0.8.1.0</a>
  <a class="md-button" href="../INSTALLATION/">Installation</a>
</div>

The download is the maintainer's complete ZIP, published unchanged with a
versioned filename. It includes Helpy and all bundled plugins. Install UE4SS
separately; preserve your settings, mods and saves when updating.

## Unpublished development follow-up

The [runtime folder restructure](RUNTIME-LAYOUT.md) separates core plugins under
`dlls/plugins` from Helpy under `Helpy`. It is a separate local candidate and is
not included in the published download above. The same local follow-up adds a
[native Helpy Mod Menu 2 plugin](MOD-MENU-2-AND-HUD.md) and
[controller profiles, touchpad gestures and glyph feedback](CONTROLLER-HUD-BINDINGS.md#reconfigure-a-detected-controller).

The independent [Developer Tools plugin](DEVELOPER-TOOLS.md) is also in local
development; its additional native gameplay commands are not yet complete.

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

## Local changes awaiting a new package

- **Mod licenses:** Select RuneSchema or UE4SS mod cards for a gold border, then
  open **License** at the bottom of the internal navigation. Read, copy and
  refresh bundled notices, or open the author's linked mod page. Compact UE4SS
  rows also support selection. See [selected mod licenses](HELPY-STANDALONE.md#selected-mod-licenses-in-development).

- **Direct item delivery:** Right-click Give waits behind background cart validation
  and sends only the requested items. Existing cart contents and selection remain
  unchanged; a disconnected recipient or world change cancels queued delivery.
- **Player mark synchronization:** An unavailable owned-player bridge uses bounded
  retry backoff and one warning per outage. Only submitted requests count toward
  acknowledgement retries. Local marks remain saved.

These changes are available in local test builds; a new download has not been published.

## Testing and feedback

Package integrity, the compressed core DLL, licenses, versions, matching source
files and strict documentation checks passed. Exact hashes and evidence are in
[the release verification files](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0).

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
