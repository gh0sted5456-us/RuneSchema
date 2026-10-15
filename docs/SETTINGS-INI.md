# RuneSchema settings

New packages ship `RuneSchema/settings/settings.ini`. Edit it with the game
closed, then restart. The same format works on Steam/GOG, Game Pass and servers.
Plugin-owned settings, mod definitions and authoring exports keep their existing
JSON/JSONC formats.

```ini
[RuneSchema]
advancedLogging = false
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
