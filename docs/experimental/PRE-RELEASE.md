---
channel: experimental
promote_to: RELEASE-0.8.1.0.md
---

!!! info "Pre-release · 0.8.1.0"
    Candidate for testing. Stable 0.8.0.3 remains available separately.

# RuneSchema 0.8.1.0 candidate

[Download the complete ZIP](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.1.0/RuneSchema-0.8.1.0.zip)
· [Release and verification files](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.8.1.0)
· [Installation](INSTALLATION.md).

The download is the maintainer-supplied `RuneSchema.zip`, published with a
versioned filename and unchanged contents. It includes Helpy, all bundled plugins,
Lua companions and release settings. UE4SS is installed separately.

## Highlights

- Helpy desktop/overlay, consistent two-pane menus and controller navigation.
- Offline Item Bay browsing, cached searches, multi-selection and carts.
- Sequential mod-cache builds, metadata editing and optional UE4SS loader controls.
- Save recovery, cleanup notices, diagnostics and update notifications.
- Host/client mod comparison, saved player authorization and optional automatic
  authorization for connections. Native Admin remains separate.
- Shared HUD, local mod-profile and presentation interfaces for mod authors.

Use the [feature index](index.md) for setup and the
[changelog](CHANGELOG-0.8.1.0.md) for development history.

## Replaced instructions

- ~~A complete 0.8.1.0 runtime ZIP is still being assembled.~~ Use the release ZIP above.
- ~~Install the old `pre-release-binaries` follow-up.~~ It is superseded by the complete ZIP.
- ~~Helpy is copied into AppData to run.~~ It runs from its installed runtime folder.
- ~~UE4SS inventory is always read-only.~~ External management is opt-in.
- ~~Submit a bug-report form inside Helpy.~~ **Report a bug** opens Nexus Bugs.

## Known limits

This is a Windows prerelease. Proton/Wine, every physical controller/driver,
authenticated multiplayer actions and server restart behavior need further live
coverage. World recovery is experimental and off by default. Mod Menu 2 remains
optional/key-gated and avoids the separate Mod Menu's hooks.

The loading indicator uses the previous startup behavior; the unsuccessful
post-intro timing experiment is excluded. Leftover Helpy-process reports remain
under investigation. Use **Exit Helpy** before replacing its files.

Normal guides stay tied to the stable release. Promotion follows
[documentation channels](../DOCUMENTATION-CHANNELS.md).
