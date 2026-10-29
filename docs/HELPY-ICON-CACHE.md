# Helpy icon cache

For typed item/journal metadata, exported paths and persistence IDs, use
[content cache bundles](CACHE-BUNDLES.md). The default export is now
`.rsmk-cache`; the legacy format below remains supported for pictures only.

Mod authors can [build `cache/` directly from cooked PAKs](modders/offline-helpy-cache.md)
with the optional Bash tool while the target game is closed. Updated Helpy's
base-game downloader checks an independent GitHub catalogue; a changed build
can reuse native pictures as display references while current item data is
discovered normally.

Helpy stores captured item icons under
`%LOCALAPPDATA%\RSDragonwilds\RuneSchema\<platform>\cache\item-icons`
(`steam`, `gamepass`, `epic` or `server`).
Tools > Utilities > Item Cache provides **Clean Icon Cache** and **Open cache folder**
in both simple and advanced modes. The link opens Windows File Explorer for
manual management. Close Helpy/the game before manually removing files; visible
icons can otherwise be captured again while browsing. Cache cleanup does not
remove mods, settings, saves, recipes or journal progress.

The Item Bay's **Cache...** button opens **Content cache**. **Export current cache...**
saves a `.rsmk-cache` bundle; **Import cache...** also accepts legacy `.rsmk-icons`
files for matching pictures. An export
does not capture every texture in the game. Normal browsing tries the compatible
local disk thumbnail before submitting a native capture request. Missing or
invalid thumbnails fall back to the game's capture pipeline and are saved for
later use when persistence is enabled. No all-assets scan runs at startup.

Item Bay, inventory gear changes and Character Editor share the same cached
catalogue and icon store. Disk thumbnail reads and index updates run off the UI
thread; at most one completed disk thumbnail is uploaded per UI update. Buttons
keep their geometry while artwork arrives. A real source change still invalidates
its entries, and native equipment/use checks remain authoritative.

Helpy owns and maintains this writable AppData cache, not the mod's installation
folder. A mod can supply optional `cache/*.rsmk-icons` packs inside its RuneSchema
mod folder. Matching active-source pictures and relationships are copied into
Helpy's cache; the original packs are never edited and do not register items.
Keep **Keep icons between launches** enabled for persistent thumbnails. The cache
window's **Open Helpy cache folder** link shows the actual writable location.

Unrelated source discoveries preserve already loaded icons and their queued or
in-flight capture requests. Immediately before a new capture, Helpy rechecks for
an imported thumbnail that arrived after the original disk miss. Real source
changes still invalidate that source's pictures; cache/export-directory writes
do not count as content changes, regardless of directory-name capitalization.

The visible item page stays in place during a same-query refresh. Empty,
non-final cache-read responses are not displayed as zero results; replacement
rows are applied together when available. A completed empty search can still
show no matches. Changing the query, world or actual content source invalidates
old rows normally. `[PH]` placeholder items are hidden by default; authors can
enable **View > Show [PH] placeholder items** without rebuilding the disk cache.

Both cache operations and **Full scan game catalogue** have live progress and
cancel controls. Export separates discovery, content verification, pixel
validation, writing and final publication. Source hashing reports MB as chunks
are read; writing reports icons and bytes. Unknown totals/final publication use
an animated bar rather than a fabricated percentage. A successful export only
finishes after the final file is published; cancellation preserves an existing
destination and removes the partial file.

## Build from the main menu

**Build registered icon cache** is an explicit capture job, separate from
exporting pictures that are already cached. It can use the existing front-end
renderer at the main menu or the active world renderer; it does not open a
gameplay world, create a character, or modify a save. If no stable renderer or
registered source identity is available, the job waits and reports that state.
The normal spawn/icon service must be enabled (the default). A dedicated
headless runtime cannot capture GPU thumbnails; compatible pack import/export
remains available.

Choose **All registered sources** or one source before building/exporting.
Exact indexed package/container ownership takes priority. Where that evidence
is unavailable, conservative declared/virtual-root grouping remains a fallback:

- `core`: ordinary `/Game` and `/Engine` assets.
- `mods/<folder>`: declared managed-mod ownership, `/Game/Mods/<folder>` or
  `/Game/RuneSchema/<folder>` content, including known schema-created assets.
- `plugins/<mount>`: other plugin/GameFeature content roots without a declared
  managed-mod owner.

The index helps identify the supplier; live discovery checks that Unreal mounted
it. When several sources could own an asset, Helpy keeps the ownership unresolved
until it can check, rather than choosing one from mount order. References to
core icons do not relabel all core content as belonging to the referencing mod.
Native plugin/GameFeature container files and descriptors participate in source
comparison when present; packed features also depend on their game containers.

Discovery is time-sliced and bounded: loaded registered DataAsset icon
references plus mounted UI/icon texture metadata. It does not force-load every
item/recipe/actor or extract every art texture. Capture reuses compatible disk
pixels first and fills missing entries through the native renderer. Progress
shows discovery, source verification and capture, with cache-hit/captured/failed
counts and cancellation. Completed thumbnails survive cancellation. Source
changes stop the batch instead of mixing generations. Increase the disk limit
or select a smaller source if the discovered batch exceeds the configured limit.

For an optional ColorsOfMoney download, choose `mods/colorsofmoney`, build,
review any failures, then dump/export. The suggested filename is
`colorsofmoney.rsmk-icons`. Physical cache pixels remain deduplicated; the
source-filtered attachment is a pack file, not a duplicated raw mod folder.
Imported packs preserve source labels for later filtered exports.

The full scan has its own native stage/entry counters and a separate cancellation
action; cancelling it does not cancel a delivery or journey. It discovers and
classifies catalogue entries, not every possible texture. Full asset loading
requires explicit acknowledgment. Settings prevents starting a scan and a cache
import/export simultaneously. Completed captured icons can be exported without
forcing another catalogue scan.

## Source comparison

New captures are grouped by content source and source fingerprint. The native
core publishes copied source metadata only after successful registry auditing;
the desktop UI does not infer installed items from disk-cache contents. Native
item-to-icon references can introduce a separate icon source, inheriting the
item source's contributing owners. Such icons wait for the updated manifest
instead of exhausting capture retries while the comparison is pending.

The filesystem-only comparison worker checks the storefront lane, game binary
and content metadata, and the contributing managed mod files. Small authored
files include a content hash. Disabled/unregistered managed mod folders are not
scanned. Shared mounts combine contributing owners conservatively. Unrelated
RuneSchema/Helpy loader changes do not invalidate every native icon. File size,
modification time and bounded container metadata participate in lightweight
comparison; explicit portable pack operations can hash full content. These are
cache-coherency checks, not mod-registration or gameplay certification.

Icons remain demand-cached as visible content is browsed. The cache-file count
is not the number of items, recipes, or available game/mod textures. No full
asset load or GPU render sweep occurs at launch. Old flat cache files are not
trusted across the layout change; they can be removed with the cleanup control.
Existing stale source generations remain bounded by the global cache limit.

Advanced mode exposes the disk limit (default 200 MB). Turning persistence off
uses `item-icons-live` for session-only captures. Cleanup checks the exact cache location and leaves redirected or junction trees
alone, keeping the operation within the intended folder. Cached pixels are installed
only from the expected per-icon path and never create catalogue entries or
prove that a mod's content registered successfully.

## Optional shared icon packs

The **Content cache** window has **Import cache...** and **Export current cache...**. Players
can also drop one `.rsmk-icons` file onto the Helpy window or detached Item Bay.
Unpack ZIP downloads first. Keep icon persistence enabled; the configured disk
limit still applies. An import that would exceed that limit asks you to increase
it or clean old generations first. Clean Icon Cache is disabled while a pack
operation is running; do not manually delete the cache during an operation.

For authors: build a selected registered source or browse its items, then Export. Only captured,
indexed icons for currently registered sources are exported; this does not
render every game texture or force-load every asset. Older thumbnails acquire
their export index when browsed again. Share the resulting file as an optional
download alongside the actual mod. The pack contains canonical icon paths,
source fingerprints and raw thumbnails, not your install paths, scripts, items,
settings or saves. It is not a replacement for the mod or PAK.

Imports check SHA-256 content identities rather than another PC's installation
path, storefront binary or file timestamps. Game content and contributing mod
files must match; missing or changed sources are skipped independently. Shared
mounts and game-container changes invalidate conservatively. Compatible pixel
files are rekeyed into the player's own source cache. Only the live catalogue
can supply items; a pack cannot register, unlock or enable anything.

The first explicit import/export may take time to hash game containers. This
runs on a cancellable filesystem worker, not the game/render thread. Local
file-size/timestamp memoization reuses those content hashes for later pack
operations. Normal launch keeps the existing lightweight source comparison;
it does not hash all large containers. A progress indicator and result show
imported/skipped counts. Workers are cancelled/joined before DLL unload.

### Bundled mod caches

Authors can ship a source-filtered pack inside
`RuneSchema/mods/<ModName>/cache/<ModName>.rsmk-icons`. Helpy discovers direct
`.rsmk-icons` files after the native source manifest is ready and imports them
on a background worker. Restart after adding a pack to a running session.
Only active, declared mod roots participate; inactive folders are ignored.
An unchanged source snapshot is attempted once per session, not every frame.
Automatic passes with zero new icons and no warnings stay silent; explicit
Import/Export operations still report their results.

The saved item-name catalogue uses the content catalogue fingerprint rather
than the broader icon-census revision. Icon-only source refinements therefore
do not discard the whole sorted item list. Real content changes, runtime clone
paths and language changes still invalidate it. A matching older manifest can
migrate without a native rescan. Icon packs contain thumbnails/source mappings,
not the item-name catalogue, and cannot make unregistered items appear.

Automatic imports use the same content and pixel checks as manual imports,
restricted to that mod's declared source roots. This includes canonical
icon-to-source relationships, not item definitions or quest state. A pack cannot
claim unrelated content merely by changing its source labels. Imported icons
are reusable disk thumbnails; missing icons still use native capture. Large
container hashes may be needed on the first bundled import, off the render
thread. A newly mounted/changed source is rechecked independently.

Discovery is bounded to 1,024 mod folders, 256 direct cache-folder entries per
mod, and 64 packs per pass. Links/junctions and unsafe paths are refused.
Malformed or incompatible packs produce warnings without disabling the mod.

The version-1 format is `RSIPACK1`, a bounded JSON manifest and consecutive RSI3
pixel blobs with per-icon SHA-256. It permits at most 32,768 icons, 256x256 pixels
per icon, an 8 MiB manifest and a 512 MiB total file. All blobs validate before
the first icon is published. It does not extract arbitrary archive filenames;
cache targets are generated locally and links/junctions are rejected. These
checks prevent malformed packs from writing outside the cache, not from
containing unattractive or intentionally misleading pictures. Download packs
from authors you trust.

## How Helpy checks cached pictures {#validation-boundary}

Automated fixtures cover source-local invalidation, unchanged-source reuse,
disabled-source exclusion, malformed manifests, cross-source dependencies,
disk reuse without a native capture, unknown-source waiting, wrong-path
rejection, and cleanup target containment. Standalone previews and DLL lifecycle
checks are not evidence that every live mod icon renders correctly; an in-game
browse/restart check remains necessary before making that claim.
The menu-cache candidate additionally tests path/source grouping, source-filtered
export/import label retention, plugin-container invalidation and menu-independent
control state. The October 17 Steam test captured all 15,352 icons discovered
for its mounted content with no final capture/discovery failures. The 247.6 MiB
full pack passed per-blob checksum validation, an isolated import of all 15,352
icons, and a second import reporting all 15,352 unchanged. These counts describe
that installation, not a promise that every possible texture is an item icon.
