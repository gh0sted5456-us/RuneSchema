# Optional Helpy UI artwork

The immutable `v1/` folder is a loose GitHub download. RuneSchema and Dragonwilds
logos are also bundled with the lean runtime ZIP so branding works offline from
first launch; the remaining UI artwork is optional. This folder contains only
Helpy's logos, platform/community marks, loading artwork and
destination preview screenshots, not game item caches, fonts, DLLs or scripts.

**Download Initial Assets** fetches the compiled list of PNGs over HTTPS and
checks exact sizes and SHA-256 hashes. Files stay in
`%LOCALAPPDATA%/RuneSchema/HELPY/ui-assets/1/` for future launches. No download
occurs before a click. Enter works offline. The centered splash prompt can be
hidden persistently; its confirmation points to **Settings > Updates & runtime >
Initial assets**, where the download remains available. Initial Assets also fetches
an optional Wiki/Wildsforge map-filter cache, with its separate CC BY-NC-SA 3.0
attribution. Map-filter data is retained until a confirmed local game-build change;
there is no timed online map polling. Game item-icon cache packs remain separate.
Failed downloads retain existing files; the version is marked ready only after
every image verifies. Repair skips unchanged images. See `v1/manifest.json` and
`v1/NOTICE.txt` for inventory and attribution. Changed artwork needs a new version
and updated compiled pins; never silently replace an immutable version.

RuneScape: Dragonwilds artwork belongs to Jagex; platform and community marks
retain their owners' rights. Screenshots are static previews, not live pages or
endorsements. Artwork is not MIT-licensed. Nexus and CurseForge owner-facing Manage
controls were removed using a built-in image edit; source screenshots are retained.
