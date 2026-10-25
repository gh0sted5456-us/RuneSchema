# Test, package and update a data mod

A valid JSON file is only the first check. Separate parsing, target resolution,
reflection, gameplay consumption and persistence when deciding whether a mod
works. Use this checklist for the [worked tutorials](fmodel-to-mod.md), not only
for a large cooked mod.

## Keep a small test matrix

| Change | Minimum useful gameplay check | Important update/removal check |
| --- | --- | --- |
| Chest capacity | Fresh and existing chest; use last slot; transfers and sort/fill. | Empty added slots before shrinking/disabling; back up the world. |
| Inventory pages | Each modified page, real final-slot storage and UI navigation. | Empty added slots; test character reload. |
| Creature loot | Correct creature/row; normal death/drop path; original drops remain. | Inspect duplicate appends and changed selectors; use enough probability trials. |
| Cooked item patch | Original identity remains; value is consumed by the intended system. | Oversized stacks, shared stat rows, donor behavior and conflicting mods. |
| Item clone | Separate identity/path; obtain, use, save/reload; donor unchanged. | Keep identity stable; test dependency removal only on backups. |

## 1. Inspect files before launch

- Keep research exports **outside** active loader folders.
- Use `.json` or `.jsonc`, not a hidden `.jsonc.txt` extension.
- Use JSONC for comments; do not assume trailing commas, duplicate keys or
  misspelled directives are accepted.
- Confirm the complete object path, row/class name and native field types.
- Generate identities only for genuinely new content. Patches preserve them.
- Include only intended fields. A normal array assignment replaces its list.
- Keep the provider and any compatibility mod in an explicit order in
  `mods/runeschema.txt`; disable duplicate copies elsewhere.

The [schemas](../SCHEMAS.md) check document structure where supported. They do
not contain every native Blueprint field, nor prove that the currently running
game still exposes a particular object/property.

## 2. Launch cleanly and inspect Diagnostics

Close the selected game/server before changing program files or PAKs. Restart
from a known mod list. In **Helpy → Diagnostics**, select the current session
and inspect issues plus the **Live Log** source that owns the message. Search
the mod folder, table/object path, property and exact diagnostic code.

Check the first failure before a cascade. "Mounted" means package mounting,
not item registration or successful gameplay. A successful write is not proof
that an already-created object has refreshed its cached state. Save relevant
log excerpts with versions/build/platform; redact personal names and paths
before putting them in a public issue.

## 3. Prove persistence and transitions

For each claimed feature, test:

1. First world entry with a new/disposable character.
2. The normal gameplay operation—not just a manual grant or a preview.
3. Return to menu and re-enter the same world.
4. Save and fully restart the game, then reload.
5. A backed-up existing character/world.
6. A game update or dependency update when you claim support for it.

For probabilistic behavior, record the number of trials and compare like-for-like
conditions. One missing drop is not evidence of a failed table write; one drop
is not evidence of the advertised rate.

## 4. Test multiplayer as a separate claim

Keep the same RuneSchema version, content providers and gameplay definitions on
the authority and clients that require them. Confirm the actual playable world
and a possessed player. Test join/rejoin, transfers, relevant permissions and
authoritative outcomes. Matching filenames or an accepted identity fingerprint
does not prove every gameplay or presentation lifecycle works.

Do not market a solo test as dedicated-server certification. Use
[client/server parity](../servers/mod-parity.md) and
[platform boundaries](../COMPATIBILITY.md).

## 5. Package the mod folder, not your working directory

For a simple lesson, distribute:

```text
MyMod/
  README.md
  blueprints/10-change.jsonc
```

Use whichever loaders the mod actually needs. Add PAK containers only when you
own/are allowed to distribute the required cooked content. Do not include game
saves, logs, account/profile data, generated mappings, cached exports, a second
UE4SS host or an entire FModel game dump. Keep third-party licensing/attribution.

The README should state tested game/store/RuneSchema versions, dependencies,
install location, enable-list entry, load order, exact intended behavior,
known limits, multiplayer coverage and safe update/removal instructions.
See [mod packaging](../MOD-PACKAGING.md) for optional cache/banner metadata.

## 6. Handle updates and removal honestly

Never regenerate a released clone's PersistenceID or silently rename its folder.
Before reducing capacity or stack limits, make existing contents fit the new
limits and retain a backup. RuneSchema's backup-first orphan cleanup is not a
generic container-resize, stack-splitting or arbitrary world-save migration.

Unfamiliar save data is retained/reported while supported verified sections can
continue cleanup. Completed quest history is normally retained. Reinstalling a
removed mod does not automatically restore possessions pruned from a save.
Follow [save safety](../SAFE-SAVE-AND-LEDGER.md) rather than testing on the only
copy of a character or promising that no possible save can become corrupt.
