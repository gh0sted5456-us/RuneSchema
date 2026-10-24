# Helpy profiles (experimental)

Profile sits beside Home in the sidebar and beside Quick settings on Home.
It contains account details and the Character workspace.
Choose a profile name once; Helpy displays `Profile name (Character name)` without
renaming the native character. The profile ID is locally generated, not a verified
Steam/Xbox identity or a server authorization credential.

The profile page shows detected platforms with the existing runtime logos.
Platform hovers show the current platform-reported account, or its last locally
observed name when playing another platform. Local Steam sessions use the game's
already initialized Steamworks persona API when available; they never use the
character name as an account-name fallback or apply the local account to peers.
Optional `PlatformAccounts` in `profile.json` retains these display names across
sessions. These names are presentation data, not authentication or permissions,
and are not added to the public network profile by this update.
Click a platform to filter linked characters; click it again to show all.
Search works within that filter. Character rows open a uniquely matching local
save copy in Character, using its native GUID and platform rather than its name.
An existing draft still requires the editor's switch confirmation. Missing or
ambiguous saves leave the draft untouched. For a character used on multiple
platforms, select the intended platform first.

Click the portrait or Picture to use the existing image-drop/crop picker.
Optional social fields are under Social links, with validated Steam/Nexus Open
buttons and a Discord Copy button. Saving details remains explicit. Reload,
draft discard and the profile folder are tucked under More.

## Local files

`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core/runeschema.profile/` is shared by
the desktop client and game/server runtimes under the same Windows account:

Startup no longer migrates file layouts or imports older AppData roots.
Existing `runeschema.profile` and `HELPY` folders directly inside the current
`RSDragonwilds/RuneSchema` AppData root are used in place when their core
counterpart is absent. An established core folder takes priority; neither
location is moved, merged or overwritten.
Native character/world saves and Xbox WGS stay in their game-managed locations.
The shared core folder also holds Helpy settings, caches and health history.
Profiles, portraits, loadouts and Recovery archives are durable user data, not
disposable caches. Deleting them is not required to rebuild generated caches.

- `profile.json`: established profile name, public links, observed platforms and
  linked native character GUIDs.
- `labels.json`: Favorite/Junk definitions and private local tags/nicknames.
- `portraits.json`, `portraits/`: account/character portrait choices and crops.
- `spawn-browser.json`: hidden spawn identities and separate resource/NPC/enemy
  browser filters. Existing Helpy `runtime/saved/spawn-browser.json` choices are
  copied once if the profile has none; the old file is retained.
- `loadouts/`: existing Helpy kit files.
- `characters/<native-character-guid>/snapshots/` and `exports/`: isolated JSON
  save copies, also discoverable from Character. Native Steam saves and Xbox WGS
  provider data remain in their game-managed locations.

The first load copies existing personalization, portraits and kits without
overwriting central files or removing originals. Copies are verified before
publication. A conflicting profile edit in another game/window requires reload;
Helpy does not silently overwrite it. Application theme, machine configuration
and rebuildable item/icon caches remain in their existing locations.

## Platform account details

The game-reported account name comes from
`DominionPlayerState.PlatformData` (`DomPlatformData.DisplayName`), not the
character name or a user-entered link. Runtime reflection verifies the struct,
string property, extent and a 128-byte visible-text limit. Empty/unavailable names
stay unavailable. Platform-icon hovers show these names; links remain clearly
self-declared. This is not independent platform-account verification.

The read-only mapping evidence for this contract is
`.cache/profile-native-contract-20261021/usmap-native-contract-20261017.json`
in the development workspace. Actual Steam/Game Pass population needs live QA.

## Public sharing

Updated clients publish only profile ID, profile name, Steam/Nexus links,
Discord username and played platforms. The native account name travels in the
server's player roster separately. Character lists, private tags, local paths,
loadouts, save contents and permission records are not part of a public profile.

Publication uses the existing owned-player bridge. The authority binds it to the
requesting controller's active native character; clients cannot choose another
character as their publication target. Reads require a connected target. Replies
are correlated to the current controller/world and request ID, with bounded
packets, retries, a 256-profile memory limit and expiry. World changes clear the
session replica. Public metadata is refreshed periodically, not every frame.

Right-click a party member and choose **View profile**. Profile metadata is
self-declared presentation, never authorization. Both ends need the updated
RuneSchema/Helpy. Live two-client/dedicated verification is still required.

## Not implemented in this build

- Network portrait/image transfer; local image drop, crop and circular portraits
  remain available.
- Server-issued/delegated View/Add/Remove/Delegate inventory grants.
- Native kick/block controls and dedicated-manager messages into in-game chat.

Those actions require verified native contracts and server-owned authorization.
Editing a profile cannot grant them. Do not use profile names, profile IDs,
character names or social links as proof of server ownership.

The agreed portrait-transfer schedule is once per session, with cached portraits
provided to newly connecting players. It should not continuously republish
profiles. This remains planned work, not a capability of this candidate.

## Sharing browser presets

Profile > Spawn browser presets offers Export and Import & merge. Presets carry
hidden entries, search text, resource category and Show hidden choices only.
They exclude placement scale/count/radius/name, player identity, links, saves,
loadouts, portraits and permissions. Export uses a `.rs-spawns.json` filename.
Import adds hidden identities without deleting existing ones and applies only
the browser filters supplied by the preset. It never spawns actors, changes
registries or grants access. Unknown/placement fields, malformed filters and
oversized files are rejected without applying them. Concurrent edits in another
Helpy window are retained instead of silently overwritten.
