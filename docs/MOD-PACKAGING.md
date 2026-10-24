# RuneSchema mod packages

Helpy's **Mods** workspace manages RuneSchema owners, not unrelated UE4SS mods
or global Unreal plugins. All order/enable changes apply on restart. Existing
mounted files are never replaced or removed by the ZIP importer.

## Recommended ZIP layout

```text
MyMod.zip
└── RuneSchema/
    └── mods/
        └── MyMod/
            ├── mod.json
            ├── equipment/       # applicable RuneSchema definitions
            ├── quests/
            ├── paks/            # cooked .pak/.utoc/.ucas companions together
            ├── logicmods/       # only if used
            ├── ue4ss/           # only if used; retain documented loader layout
            └── cache/           # verified Helpy cachepack, with source art
```

Do not invent loader directories: use RuneSchema's supported content directory
names. `paks` and `logicmods` may contain packages one level below their root.
Keep each cooked container's companion files together. Do not ship saves,
settings, the loader DLL, other mod owners or `runeschema.txt` in a mod ZIP.

A single `MyMod/mod.json` wrapper or a flat package with `mod.json` at ZIP root
is also accepted. Legacy archives with a full Steam installation prefix are
accepted **only when every payload file belongs to one unambiguous
`RuneSchema/mods/MyMod/` subtree**. The prefix is discarded; no drive letter or
installation path becomes part of the destination. Mixed-owner/core bundles
are rejected, not guessed at.

## Optional manifest for legacy packages; required for flat packages

```json
{
  "format": 1,
  "id": "MyMod",
  "name": "My Mod",
  "version": "1.0.0",
  "author": "Your name",
  "description": "A short description.",
  "banner": "art/banner.png",
  "history": [
    {"version": "1.0.0", "notes": "Initial release."}
  ]
}
```

`id` must match the destination folder exactly. Use letters, numbers, spaces,
underscores or hyphens; start with a letter or number. Windows device names,
trailing spaces/dots, path separators and control characters are not allowed.
Metadata is author-supplied, not a signature or compatibility guarantee.
Loaders shown by Helpy are inferred from actual installed folders.

Older informational `mod.json` files without a `format` field remain readable
for installed-mod presentation. New packages should declare `format: 1`; the
ZIP importer still requires its supported manifest format for flat archives.

## Names and identity

Legacy `ID.txt` remains supported. A plain one-line ModID still works. Optional
structured metadata can supply a friendly default without changing that ID:

```ini
ModID = MyMod
Name = MyMod
Nickname = My Friendly Mod
Version = 1.0.0
Build = local-20261020
Author = Your name
Website = https://example.com/my-mod
NexusModID = 123
NexusFileID =
Description = A short description
# Optional, existing mod-relative artwork:
# Banner = cache/banner.png
```

`DisplayName` is also accepted as an alias for `Nickname`. Keep the ModID stable:
nicknames never rename folders, namespaces, native item IDs or PersistenceIDs.
When supplied, `mod.json` metadata takes precedence over `ID.txt` presentation
defaults. Helpy's personal nickname override takes precedence over both.

Place `ID.txt` at the module root, beside `paks/`, `registry/` and `cache/`.
`Build`, `NexusModID`, `NexusFileID`, `Description` and `Banner` are optional.
Nexus Mod ID identifies the mod page; Nexus File ID identifies one uploaded file.
Leave File ID blank (or `Unknown`/`unset`) for an unpublished rebuild; never reuse
an older file ID as proof that a newer payload was uploaded. IDs must otherwise
be positive decimal values. `Build` is inert author-supplied build text.
`Banner` must be an existing relative PNG/JPEG/WebP, without links or `..`.
None of these fields changes gameplay IDs, grants trust, downloads content,
enables mods or starts automatic updates. Missing `ID.txt` is still supported.

For a portable cache, right-click the module in Helpy > Mods > **Export source cache**.
Use `.rsmk-cache` under `<module>/cache/` for typed item/journal/art references
and their cached artwork. `.rsmk-icons` remains a legacy pixels-only format.
An empty cache directory, `cache/ID.txt`, release notes or arbitrary thumbnail
files are not a verified Helpy bundle. Never copy the entire AppData cache.
Targeted exports require exact module ownership in shared namespaces, retain
legitimate referenced native artwork without relabelling its owner, and exclude
unrelated records and icons. Unproven records are excluded and coverage stays
partial unless a current discovery/capture survey proves otherwise.
Validate through the real importer against matching content hashes. Removing or
changing a required module must skip its dependent artwork and references.
Packaged `cache/` files do not affect the source content fingerprint.

Right-click a mod placard to open its declared website or installed folder.
An absent website stays unavailable; Helpy never guesses an author's URL.

Personal mod/item nicknames, favorites, tags and removal locks are stored in
Helpy's `personalization.json`, not in the mod package or character save. An
item lock protects every instance of that item type against Helpy removal; it
does not prevent the game's own discard, consumption or durability behavior.
Bulk removal review excludes locked, favorite, equipped and unreadable items.

## Artwork and caches

The optional `banner` field is a relative path to an image inside the same mod
folder (for example `art/banner.png`). It decorates the mod placard even while
disabled, without mounting the mod or certifying its cache. Keep the image below
20 MiB. Omit this field if the mod has no supplied art. The placard falls back to
the source's existing banner or a neutral panel. Native colored power-level
glyphs display its order number, not a gameplay power level.

Select the mod's actual `mods/MyMod` source in **Cache & banner**, choose source
art, then export a verified cachepack into the mod's `cache` directory. Recommend
1280×720 artwork with no critical text at the edges; Helpy exports its inert
thumbnail with the pack. Cache identity/hash validation still applies. Disabled
or not-yet-registered mods cannot borrow another source's identity; configure
their cache after enabling and restarting. Item banners do not mount game assets.

Ship actual verified bundles, not an empty cache directory. Captured bundles may
have partial coverage and retain their original source fingerprints: replacing
a PAK is not permission to relabel old records as freshly captured content.

## Observed mod health

Helpy separates this session's attributed errors/warnings from lifetime counts.
The core maintains local plaintext statistics in
`%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core/health/mods-steam.json`, `mods-gamepass.json` or
`mods-server.json`. This contains short issue excerpts and can include local
paths from logs. It is not uploaded. Rows disappear when their mod folder is
removed; disabling a mod retains its history.

Only the current process's matching game log is counted. If it is unavailable,
the monitor uses its own RuneSchema/UE4SS logs. Explicit owner labels take
precedence over dependency paths. Missing optional conventional registry probes
are tracked separately; rejected declarations and real asset errors remain
visible. Green means no attributed issue was observed, not certified compatibility.

## Import and safety

Imports start disabled. Review, enable, **Apply for next launch**, then restart.
The importer stages outside `mods`, limits ZIP size/expansion/entry counts,
rejects unsafe paths, links, duplicate case aliases, installer programs and
ambiguous payloads, and publishes a complete new folder via same-volume rename.
Importing does not execute archive code; enabling a script/native-code mod later
is still a trust decision. Inspect its author and source first.

Existing mods are not overwritten, even when disabled: they may still be mounted
in this process. An inspected replacement remains under `RuneSchema/mod-imports`
for offline review. Close the game before manually replacing installed files.
Automatic update/removal of existing installed folders is not implemented.

Load-order writes compare the disk snapshot and use RuneSchema's existing atomic
writer. External changes require Refresh. Green means **no attributed errors
observed in the retained log tail**, not proven healthy. Yellow means warnings
or incomplete/unavailable logs; red means attributed errors. Hover for evidence.
Generic console failures without a mod tag/owned path remain in Live Log.
