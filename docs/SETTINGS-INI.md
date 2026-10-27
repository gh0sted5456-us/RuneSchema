# RuneSchema settings

New packages ship `RuneSchema/settings/settings.ini`. Edit it with the game
closed, then restart. The same format works on Steam/Epic, Game Pass and servers.
Plugin-owned settings, mod definitions and authoring exports keep their existing
JSON/JSONC formats.

The following is a syntax example, not a replacement for the full shipped
defaults. The current package enables detailed logging and Helpy auto-opening;
existing settings are preserved during an update.

```ini
[RuneSchema]
advancedLogging = false
archiveLogs = false
authoringTools = true
helpyHotkey = "F2"

[loaders]
quests = true
spawns = true

[plugins]
compatibilityNotices = "quiet"

[helpyAuthority]
permittedPlayerGuids = []
permittedPlayerNames = ["Example Player"]
```

Keys and section names are case-sensitive and retain their JSONC names. Root
settings belong in `[RuneSchema]`; each nested settings object gets a section,
such as `[loaders]`. Deeper objects use dotted sections. Use `true`/`false`,
ordinary numbers, quoted strings and JSON arrays. Plain words are also accepted
for string settings. Semicolon and hash introduce comments outside quoted
strings. Missing keys keep their built-in defaults; unknown keys are preserved.

## Helpy shortcut

Open **Home > Quick settings** to set the **Helpy shortcut**, reset it to F2,
or choose whether Helpy opens at startup. The shortcut saves to this
installation's `settings/settings.ini` with a backup and applies immediately.
All shortcut hints use the selected key. **Reload theme** is a button in Quick
settings; there is no separate F5 theme shortcut.

For manual changes with the game closed:

```ini
[RuneSchema]
helpyHotkey = "F3"
```

Restart the game after editing the file manually. Supported single keys are
F1–F24, A–Z, 0–9, Insert, Home, End, PageUp, PageDown, Pause and ScrollLock.
Names are case-insensitive; unknown names fall back to F2. Modifier combinations
and mouse buttons are not supported. Choose a key that does not conflict with
game controls or other mods. Letters and navigation keys keep working inside
Helpy text fields rather than closing the window while you type.

The shortcut opens Helpy while this game's window is focused and closes it
while one of its own windows is focused. It does not toggle another runtime's
Helpy or change a connected server's shortcut. Other RuneSchema settings still
require a restart.

## Game mappings and writable state {#game-mappings-and-writable-state}

Keep executable content in the game's `ue4ss/Mods/RuneSchema` folder. Settings
remain at `RuneSchema/settings/settings.ini`; enabled mods remain under
`RuneSchema/mods`. Writable shared state is separate:

```text
%LOCALAPPDATA%/RSDragonwilds/RuneSchema/
  core/
    HELPY/                   # UI preferences, generated caches and UI assets
    runeschema.profile/      # Profile, portraits, loadouts and character copies
    backups/                 # Required character pre-prune backups
    health/                  # Retained diagnostic history
  steam/
    cache/mappings/          # Steam mapping snapshots/index
    runtime/saved/progress/  # Platform runtime progress sidecars
  gamepass/                  # Same platform-local separation
  epic/
  server/
```

Each game process has its own runtime connection and Helpy instance; shared
preferences are not a reason to connect Helpy to a different process. Native
character/world saves, including Xbox WGS, remain game-owned. Do not move them
into this tree or delete profiles/backups as a cache reset.

`[RuneSchema] autoUpdateMappings = true` requests a native mapping capture when
the installed game identity lacks a compatible verified snapshot. Capture is
delayed rather than added to the immediate startup path. A successful capture
is reused on the next launch; unchanged launches do not dump another USMAP.
`RUNESCHEMA_DISABLE_AUTO_MAPPING` disables automatic generation for that process.
Already verified compatible snapshots can still be reused. The startup log
distinguishes queued, deferred and successful capture; a queued request is not
an active mapping. [Mapping details](HELPY-SCHEMA-WORKSPACES.md#game-update-reflection-mappings).

There is no required `dlls/mappings` folder. Old AppData roots and game-local
`runtime/live` trees are not automatically moved or merged. Existing legacy
`HELPY`/`runeschema.profile` folders directly inside the current AppData root
are used in place only when their `core` counterpart is absent. Keep ordinary
local directories; do not redirect the root through a junction or symlink.

For generated cache locations and source separation, see
[cache ownership](HELPY-CACHE-OWNERSHIP.md) and [cache bundles](CACHE-BUNDLES.md).

## Backup-first pruning

RuneSchema keeps native saves and verifies a complete pre-prune backup before
changing supported character or world data. No companion Recovery mode or
automatic restoration is used. The former `persistence.recovery` setting is
retained for old configuration compatibility but is ignored and hidden; the
former world `backupBeforePrune` switch cannot disable required backups.

Character-load and hydrated-quest backups are stored under
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core/backups/<runtime>/characters/<GUID>`.
Identical bytes share a SHA-256-named `.json.bak`; different versions are kept.
Startup loose-file cleanup and world cleanup retain their existing verified
pre-cleanup backup paths. Backup failure prevents that cleanup attempt.

Restoration is manual and rolls progress back to the selected whole save.
Existing companion archives are neither replayed nor deleted. Save Bypass and
the separate default-appearance repair settings do not enable companion Recovery.

## Loading modes and Save Bypass

Home > Quick settings offers Normal and Quick Launch. Vanilla Load is under
Advanced. **Save Bypass** remains a separate, confirmed toggle:

- **Normal (Less Restrictive)** (default): manual selection with `dom.PersistenceStrictMode=0`
  retained across loads and menu returns for the game session.
- **Quick Launch**: the same Less Restrictive policy plus automatic selection
  of the saved character/world, loading-screen skip and camera fade after possession.
- **Vanilla Load** (Advanced): use native strictness and manual character/world selection.

Dedicated servers expose only **Save Bypass** as a loading control, not these
client modes or character/world selectors. The server still auto-starts its
configured world. It requests session-long native tolerant loading by default
and uses acceptance fallback only after native rejection.
Pruning runs in either case. Client `quickLaunch.mode` is ignored on servers.

Vanilla Load does not disable RuneSchema's registry-based orphan pruning.
Hold Shift during startup to skip automatic launch; this does not cancel the
selected session strictness policy.
If a native world load has already been requested, Shift restores the loading
screen but does not cancel that game-owned load.

Choose a character and world in Quick settings, then **Save launch options**.
You can change the selection whenever Helpy is connected; changes apply on the
next game launch, never travel out of the running world. Native save choices
are refreshed in the main menu. Both manual modes retain the previous selection for later.
Options are stored in the existing `RuneSchema/settings/settings.ini`:

```ini
[quickLaunch]
mode = "lessRestrictive"
characterName = "BREWER"
worldName = "Example world slot"
```

`mode` accepts `vanilla`, `lessRestrictive`, or `quickLaunch`. Old settings without
`mode` migrate `enabled=false` to Normal (Less Restrictive) and `enabled=true` to
Quick Launch. An explicit `mode="vanilla"` remains Vanilla Load. The legacy
`enabled` and `softLoading` keys are synchronized aliases, not separate controls.
Only Quick Launch requires selected names. Names must identify
an existing native character and world slot. Ambiguous character names, unsupported
native signatures, unavailable lifecycle hooks or a timeout stop automatic loading
without retrying. Disable the standalone QuickLoad mod before using this option.
The supplied standalone mod's INI is not rewritten or silently enabled.

The separate bypass setting remains:

```ini
[persistence]
allowCorruptCharacterEntry = false
```

Neither relaxed mode changes the saved bypass checkbox. Both automatically
override rejected returns from the paired, verified native acceptance functions,
after those functions execute normally. This includes direct native calls that
do not pass through the reflected character preflight callback. A preflight
receipt is diagnostic evidence, not permission to use the fallback. Successful
native results remain untouched; no native load is replayed or skipped.
Vanilla Load clients require the explicit Save Bypass checkbox for acceptance
overrides. Dedicated servers use tolerant loading and failure-only fallback by default.
Relaxed modes verify the
native tolerant policy at menu/world and character-load boundaries, retain it
after the load handler returns, and reapply it if a native transition resets it.
The prior value is restored only on RuneSchema shutdown when a valid native
context remains available; nothing changes the game's disk configuration or
unmodded future sessions. If the native variable or setter cannot be verified,
RuneSchema logs the failure and does not claim tolerant loading succeeded.
Registry-based orphan cleanup remains independent of the loading mode and
Save Bypass, and does not wait for all pending
clone/quest registrations. Exact declared pending identities remain protected.
Neither tolerant loading nor bypass restores unreadable progress, guarantees entry,
or proves that an in-memory prune has been persisted to disk. Keep original backups.

Quick settings keeps the mode, Save and independent Save Bypass toggle visible.
Character/world choices appear only for Quick Launch. Hover controls for details.
Manual slot-name entry, refresh/reload and less-common switches are under **Advanced**.

## Additional Logging Information

### Full-memory crash capture

**Diagnostics → Live Log → Options → Crash capture** includes **Full-memory crash
dumps (next launch)**. This is independent of Additional Logging Information.
It edits only this runtime's `ue4ss/UE4SS-settings.ini`, preserving its other
settings and comments and creating a backup before saving:

```ini
[CrashDump]
EnableDumping = 1
FullMemoryDump = 1
```

Restart Dragonwilds to apply the change. Full dumps include process memory and
can be several GB; simultaneous worker failures can create more than one file.
They stay in UE4SS's local crash-dump directory and are not uploaded by Helpy.
Review them before sharing because process memory can contain private data.
Turn the option off and restart after collecting the failure. Turning it off
restores small-dump mode without deleting existing dumps or disabling ordinary
crash capture. A conflicting settings edit requires reloading before saving.

### Optional log context

Diagnostics → Live Log offers an opt-in **Additional Logging Information** toggle and an
**Options** popover. The master switch and every field default to off:

```ini
[additionalLogging]
enabled = false
filePaths = false
region = false
networkRates = false
savePaths = false
averageLatency = false
```

The viewer saves these options with a settings backup. Changes made through its
popover apply on the next sampler tick; direct INI/advanced-settings edits apply
at restart. Context is appended only to the existing UE4SS-derived
`ue4ss/Mods/RuneSchema/RuneSchema.log`, not to a new directory or external service.
Date/time/group prefixes and existing Game/UE4SS messages are unchanged.

- `filePaths`: executable, mod, cache and log locations from existing path helpers.
- `region`: the configured Windows country/region; no GPS or IP-geolocation lookup.
- `networkRates`: observed upload/download traffic in Mbps across active physical
  Ethernet/Wi-Fi adapters. It includes other applications, not just the game. Two
  comparable samples are required; this is not a capacity estimate or speed test.
- `savePaths`: known native save-provider directories, never save contents. Active
  payload filenames are reported as unavailable when the provider does not expose
  them; Game Pass WGS ownership remains with the native provider.
- `averageLatency`: mean native player ping for an observed multiplayer cohort,
  with sample counts and server/client scope. Zero, invalid and unavailable values
  are excluded; an empty sample is **unavailable**, not zero latency. Client
  replicated values may be stale.

Paths/region are emitted when an enabled selection changes; rates and latency
sample every 30 seconds. Native ping reads are sliced across game ticks. Turning
the master switch off stops new additional samples, but does not erase prior
rows or redact paths already present in ordinary native/mod messages. Review logs
before sharing, particularly when either path option is enabled.

## Continuous logs and optional archives

`RuneSchema.log` continuously records the current run in the mod root, including
normal RuneSchema messages. `advancedLogging=true` adds detailed RuneSchema-only
diagnostics without adding that verbosity to UE4SS's console. The log is not
split or replaced at an 8 MiB boundary.

Set `[RuneSchema] archiveLogs = true` to preserve the previous nonempty run at
the next launch as `logs/RuneSchema-YYYYMMDD-HHMMSS-mmmZ.log` (UTC archival time).
Archive names never intentionally overwrite an earlier archive. The active file
remains `RuneSchema.log`. Archives have no automatic retention/deletion policy;
manage disk space yourself. With archiving off (the default), a normal launch
starts a fresh active log. An invalid settings file appends the diagnostic
session instead of discarding the previous log. A failed archive leaves the old
file untouched and reports through UE4SS. This option does not change UE4SS.log.
Helpy exposes it in Settings > RuneSchema and Home > Quick settings. Restart is
required; merely opening or clearing Live Log never changes the file on disk.

## Automatic, two-launch migration

This section describes **settings format translation only**. It does not move
the installation or migrate old runtime folders into AppData.

1. With a legacy `settings.jsonc` present, the first launch loads its values,
   writes and verifies `settings.ini`, and records a private migration receipt.
   A shipped default INI does not override the user's legacy settings. The JSONC
   stays byte-for-byte unchanged for this launch. If a different INI already
   exists, it is backed up before translation.
2. On the next launch, a valid INI becomes authoritative. If the JSONC still
   matches the version translated on the previous launch, RuneSchema verifies
   a `settings.jsonc.migrated-<unique>.bak` backup and removes the old active
   `settings.jsonc` plus its receipt. Its original comments remain in the backup.

JSONC-only configurations remain supported. If migration cannot finish, the
current session continues using JSONC and leaves it intact. An invalid generated
INI defers cleanup instead of discarding the valid legacy file. JSONC edits
between launches are retranslated if the INI is unchanged; conflicting edits to
both files keep both files for manual review and use the valid INI. Invalid
receipts never authorize cleanup. Locked/unwritable files defer migration or
cleanup without interrupting the session.

Do not distribute `settings.ini.migration` or `.bak` files. They are local
migration/recovery state, not settings templates. Updating the framework should
preserve existing settings and must not recreate legacy JSONC beside a migrated
INI. The historical JSONC template remains in the source tree only as a
compatibility fixture; release packaging copies the INI instead.
