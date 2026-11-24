# RuneSchema 0.9.0.7HF

Hotfix prerelease · November 23, 2026.

[Download RuneSchema 0.9.0.7HF](https://github.com/gh0sted5456-us/RuneSchema/releases/tag/0.9.0.7HF).
This is a complete RuneSchema/Helpy runtime update based on
[0.9.0.7](RELEASE-0.9.0.7.md). The release also provides the paired Settings Menu
runtime needed for PIN linking. Install both components for that feature.

## Changes

- A successfully verified Settings Menu server PIN grants saved full Helpy
  administrator authorization to the connected character. The server stores
  approval in RuneSchema's existing authorized-player list.
- Helpy's own successful configured PIN also saves full authorization instead of
  only a temporary PIN session. PIN approval uses native character identity.
- Saved Helpy administrators can manage RuneSchema player authorizations in the
  same world. Native game Admin changes retain their native-owner requirement.
- RuneSchema's settings definition is now `sm.json`. The paired Settings Menu
  preserves JSON literal types inside RuneSchema's INI file and retains private
  and unknown fields. RuneSchema also accepts older 1/0 INI boolean toggles.
- Authorization changes use the existing settings notification and policy-push
  path. No additional polling is introduced.

Existing item identity, quantity, delivery-flag and receipt checks remain active.
A saved approval lasts until revoked; rotating the PIN or disabling new linked
approvals does not remove previously saved players. A different character needs
its own approval. See [setup and revocation](experimental/SETTINGS-MENU-ACCESS.md).

## Installation

Close Dragonwilds and exit Helpy. Install the complete RuneSchema runtime and the
paired Settings Menu runtime in the UE4SS Mods folder. Preserve each installation's
settings, mods and activation keys. Stock UE4SS is installed separately.

Public downloads contain no Mod Menu activation key. That optional frontend uses
an installation-specific key; preserve yours. Server PINs and authorized players
are not included in the download.

## Verification scope

The Windows native builds, focused signed-PIN/configuration tests and
configuration reload tests passed. Settings Menu's controls were inspected at
1280x720 and 960x640. The dedicated-server unlock, actual restart and rejoin
acceptance test remains pending; this release is marked prerelease.
