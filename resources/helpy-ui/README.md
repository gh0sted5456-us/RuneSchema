# Optional Helpy UI artwork

The immutable `v1/` folder is a loose GitHub download, not part of the lean runtime
ZIP. It contains only Helpy's logos, platform/community marks, loading artwork and
destination preview screenshots, not game item caches, fonts, DLLs or scripts.

**Download / repair UI artwork** fetches the compiled list of PNGs over HTTPS and
checks exact sizes and SHA-256 hashes. Files stay in
`%LOCALAPPDATA%/RuneSchema/HELPY/ui-assets/1/` for future launches. No download
occurs before a click. Enter and the code-drawn fallback interface work offline.
Failed downloads retain existing files; the version is marked ready only after
every image verifies. Repair skips unchanged images. See `v1/manifest.json` and
`v1/NOTICE.txt` for inventory and attribution. Changed artwork needs a new version
and updated compiled pins; never silently replace an immutable version.

RuneScape: Dragonwilds artwork belongs to Jagex; platform and community marks
retain their owners' rights. Screenshots are static previews, not live pages or
endorsements. Artwork is not MIT-licensed. Nexus and CurseForge owner-facing Manage
controls were removed using a built-in image edit; source screenshots are retained.
