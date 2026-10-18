# RuneSchema settings

New packages ship `RuneSchema/settings/settings.ini`. Edit it with the game
closed, then restart. The same format works on Steam/Epic, Game Pass and servers.
Plugin-owned settings, mod definitions and authoring exports keep their existing
JSON/JSONC formats.

```ini
[RuneSchema]
advancedLogging = false
archiveLogs = false
authoringTools = true

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
