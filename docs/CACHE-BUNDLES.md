# Shared content cache bundles

Share a `.rsmk-cache` bundle to give Helpy a head start on item names, pictures
and other browsing details. A bundle includes exact Unreal asset paths, known
persistence IDs, source ownership, item metadata, journal text and cached artwork.
Install the mods separately; a cache contains browsing data rather than executable
mod definitions.

!!! note "Availability"
    GitHub hosts version-2 native item/journal bundles as well as legacy
    icons-only packs. The current Steam hotfix cache
    (`++dominion+hotfix-CL-254097`) contains **2,903 item/journal records and
    4,246 pictures**. Coverage remains partial; this is not a full-game census.
    Updated Helpy reads the current data-only catalogue independently of the
    DLL's release tag.

## Use an existing cache immediately

The loading-order and passive-update controls below are included in
**0.8.0.3**. Existing caches and preferences remain usable when updating.

Open **Home > Quick settings > Content & cache** to choose how the item
catalogue loads. These preferences are saved in Helpy's `ui.jsonc` and apply
to the next item query, including subsequent launches.

| Preference | Behavior |
| --- | --- |
| **Cache first (recommended)** | Show compatible saved/imported references immediately. Discover changed native content in bounded slices when passive updates are enabled. |
| **Game first, cache fallback** | Build the current loaded-item catalogue in bounded slices. Use saved references while a live page is unavailable or has no matching items. This does not load every asset from every PAK. |
| **Passive content updates** | Allow the item catalogue to continue discovering content between requests. When disabled, searches and explicit refresh still work. Source verification, visible-icon requests and live action validation remain active. |

With Cache first and passive updates disabled, an available imported page can
be browsed without starting a native catalogue sweep. Missing artwork can still
be captured on demand. Persistent artwork remains controlled separately by
**Keep icons between launches** and the cache size limit.

**Passive content updates defaults to enabled.** A search with no cached matches
falls through to a targeted live lookup, including after a large saved catalogue
has been scanned in several pages. Matching live rows are written asynchronously
to that platform's AppData `cache/content-bundles` store. Later searches and
launches reuse them without waiting for a complete catalogue rebuild. These
partial observations do not mark the catalogue complete or grant permission to
spawn an item. Disabled or removed providers remain excluded.

Discovery also checks the exact package inventory of enabled containers when
the native item registry omits mod assets. `ITEM_` package names are presentation
candidates; the actual native type and availability are still checked when used.
Learned references survive installation-folder and game-content version changes;
their current source ownership is checked again before display.

Quick settings keeps **Open Helpy at startup** visible. Game launch, save
bypass and the existing advanced controls are under **Game launch & saves**.
Those launch/save options still apply on the next game launch; changing the
content-loading preference does not change save policy.

Diagnostics records the selected policy as [RS-CACHE-011](ERROR-CODES.md#rs-cache-011)
and game-first cache fallback as [RS-CACHE-012](ERROR-CODES.md#rs-cache-012).
[RS-CACHE-013](ERROR-CODES.md#rs-cache-013) identifies an unsupported or malformed
content preference. These codes have their own **Cache** category and linked
troubleshooting reference. Policy/fallback messages are emitted on transitions,
rather than once per frame.

For manual configuration, use this object inside `ui.jsonc`:

```json
"cache": {
  "iconCacheMB": 200,
  "loadOrder": "cacheFirst",
  "passiveRefresh": true
}
```

The other supported `loadOrder` is `gameFirst`. Existing configurations that
omit the new fields retain Cache first with passive updates enabled.

1. Open **Content cache** and choose **Download base-game cache**, or **Import** a local bundle.
2. Watch download byte progress, checksum validation, and relationship assignment.
   Import does not wait for a mounted-content census, character or world.
3. Items, names, known IDs and pictures are saved in the **current platform's
   AppData cache**. The item browser reads this store directly on a file worker,
   even while the live catalogue connection is unavailable.
4. With passive updates enabled, Helpy requests a bounded native refresh at
   most once per 15 seconds while cached browsing is active. The native worker
   advances discovery in small slices; newly saved native observations can
   update the displayed cache. A game-version change is not required.

After import, the open item list and journal references refresh automatically.
On later launches, the same platform store is read immediately. A Steam cache
does not become Game Pass's cache: download/import it in that runtime too.
Profiles and shared UI assets remain under `core`; caches are platform-local.

A downloaded cache is a partial starting point, not a complete inventory of
enabled mods. A search with no cached match immediately falls through to a
bounded native item search, including modded items. Native pagination stays on
that search. Passive discovery also runs when no cache was hit; an asynchronous
cache read must not leave the native catalogue waiting forever.

Saved native discoveries are merged with downloaded data. Downloading a newer
vanilla bundle does not hide mod items already learned from the current game.
Once the native catalogue is saved, subsequent launches reuse those discoveries.
Saved native disk pages are read directly for browsing and searches. Helpy does
not rebuild the whole JSON catalogue for each query; its in-memory sorted index
is also reused until the underlying cache or active sources change.

PAK/IoStore directory discovery accepts installed content behind Game Pass or
mod-manager directory redirects. It only reads bounded, validated directory
metadata. Cache writes retain their separate protection against redirected
destinations. A detected PAK supplies attribution, not permission to grant items
or skip live gameplay validation.
Source fingerprints control revalidation, rather than the game version alone.

Only enabled source owners contribute mod records. Before the current owner
list is available, cached vanilla content can still display; mod references
wait for that list, without requiring a world login or native item sweep.
Disabling/removing a source hides its cached records and saved native rows;
its files can remain on disk for later re-enabling. Packaged cache imports keep
the containing folder as an owner alias, so renaming an installed mod does not
change the publisher's content identity.

Allow a moment for the first file read, which runs in the background. A cached
item is a **presentation reference** for browsing; when you give, equip or use it,
Helpy checks the running game for availability and permissions. Install and enable
the supplying mod to register its content.

Download selection prefers the current game build and runtime, but a different
game version does not block display of a previously published cache. A cache
in another language remains readable as a fallback; it is not relabelled as a
new translation. The preferred local language wins when available.

Native background discovery keeps its 512-slot / cooperative 500-microsecond
slice budget and 50-ms minimum interval. Missing visible pictures use the
existing limited capture queue. Thumbnails load on a separate file worker,
with at most one completed texture upload per UI frame.

## Export and share

**Export cached bundle** does not require a full sweep. It exports currently
compatible cached item/journal records and indexed pictures. **Build / update**
still performs the existing full icon-discovery/capture workflow. Advanced
**Build missing icons, then export** runs that workflow before exporting.

A full icon survey is not a full item/journal dump. Record coverage remains
explicitly partial. Missing persistence IDs are exported as an empty string
with `PersistenceIDKnown: false`; they are not replaced by an asset name.

Mod authors may include `cache/*.rsmk-cache` inside their RuneSchema mod folder.
To generate that folder directly from cooked PAKs while the target game is
closed, use the [optional offline cache builder](modders/offline-helpy-cache.md).
Automatic imports read supplied bundles into the platform's presentation
library without making their metadata a verified source. Publisher owner
labels remain routing labels; the installed mod folder's spelling does not
determine cache validity. Native ownership proofs remain required for verified
exports and original-provider attribution. Modifying a vanilla item is not a
new original-provider claim.

Legacy `.rsmk-icons` files still import/export preview pixels. They contain no
item identities, persistence IDs or journal text.

## Who owns the files?

Mod-supplied files are **read-only inputs**. Helpy never writes an updated bundle
back into an installed mod. Local captures, repaired metadata and verification
results belong to Helpy's own writable AppData cache.

| Location | Role |
| --- | --- |
| `RuneSchema/mods/<owner>/cache/*.rsmk-cache` | Author-distributed input bundle; never rewritten |
| `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/<platform>/cache/content-library` | Imported item/artwork relationships, source fingerprints, revisions and atomic index |
| `…/content-library/pixels` | Content-addressed, checksummed thumbnail pixels |
| `…/cache/item-icons` | Current native thumbnails and source snapshot |
| `…/cache/item-catalogues/native-items.json` | Saved native catalogue, independent of the installation folder name |
| `…/cache/journal` | Native journal presentation |
| `…/cache/downloads` | Verified download staging, removed after a successful import |

`<platform>` is `steam`, `gamepass`, `epic` or `server`. Supplied mod bundles
and other platforms' stores are not modified. Existing old cache folders are
left alone; no migration of profiles or user files is needed for this layout.

An author releases a replacement bundle with a mod update. Local imports keep
a source fingerprint and monotonic local revision. The offline tool also emits
`CacheVersion`: identical exports retain their revision, changed exports
increment it. A newer revision replaces that source's records and artwork,
including removed entries. Other sources and languages remain intact. Old
versions cannot roll back a newer published revision.

For a family of mods, ship each owner's bundle in its own `cache` folder.
A combined bundle may also be imported manually. Owner labels route references;
they do not grant gameplay permissions or prove current availability.

## Bundle format

| Field | Meaning |
| --- | --- |
| File header | Eight ASCII bytes `RSCACHE1`, little-endian uint32 manifest length |
| JSON manifest | Version 2, Kind `RuneSchemaCacheBundle`, Locale, Sources, Records, Icons |
| Sources | Asset-source group → portable SHA-256 of native content and contributing owners |
| ContentOwner (optional) | One canonical publisher `mods/<owner>` label. Records cannot escape that owner; the label is independent of the destination installation folder. Strict export verification still checks current ownership. |
| Records | Typed presentation references, not executable item/journal definitions |
| Icons | Exact texture path, source group, SHA-256, byte length, optional Origins |
| Payload | Consecutive bounded `RSI3` RGBA thumbnails, in Icons order |
| Coverage | Icon coverage; record coverage is separately partial |

Each record has `Kind` (`items`, `journal`, `art`, `portraits`, `resources`),
`Owner` (`core`, `plugins/<provider>`, `mods/<owner>`), `Source`, `Path`,
`PersistenceID` and a bounded `Data` object with `Name`.
Paths and IDs are copied exactly. An icon path is an artwork relationship, not
the record's source provider.

Example **illustrating the structure, not real game IDs**:

```json
{
  "Kind": "items",
  "Owner": "mods/example",
  "Source": "examplemount",
  "Path": "/ExampleMount/Items/DA_Example.DA_Example",
  "PersistenceID": "",
  "Data": {
    "Name": "Example item",
    "Icon": "/ExampleMount/Art/T_Example.T_Example"
  }
}
```

Metadata allows only presentation fields. Runtime object handles, commands,
claimed availability, unlock/read flags, character data and arbitrary
definitions are dropped. Journal pages are bounded plain text.

## Compatibility, safety and performance

- Import validates the bundle structure, typed fields and every pixel checksum
  before changing the library. It never loads Unreal assets or executable code.
- Display import does not hash installed PAKs or wait for live owner RPCs.
  Native source verification remains separate for captures, exports and actions.
- GitHub downloads are explicit; no background network sweep is introduced.
  Local mod-cache discovery is bounded and checks at most once per minute.
- Immutable records/pixels are published before an atomic index commit.
  Cancellation, corruption, write contention or a budget failure keeps the
  previous index. Only unreferenced files owned by the old index are retired.
- Same-fingerprint partial data can merge; a new source revision replaces its
  old data. Identical reimports reuse existing files. Other owners are retained.
- Limits remain 512 MiB per bundle, 80 MiB manifest, 64 MiB / 65,536 metadata
  records, 32,768 pictures and 256×256 thumbnails. Cache size limits still apply.
- File reads, checksum checks and JSON filtering run on workers. Unchanged
  indexes reuse parsed references and picture relationships.
- Servers can display imported item references and pictures. This never adds
  missing server definitions or allows native icon capture on a headless GPU.

Useful diagnostics include [RS-CACHE-002](ERROR-CODES.md#rs-cache-002)
(invalid bundle), [RS-CACHE-003](ERROR-CODES.md#rs-cache-003) (identity mismatch),
[RS-CACHE-004](ERROR-CODES.md#rs-cache-004) (integrity),
[RS-CACHE-008](ERROR-CODES.md#rs-cache-008) (unproven owner) and
[RS-CACHE-010](ERROR-CODES.md#rs-cache-010) (legacy pixels only).

## Include a source banner

1. Open Helpy's **Content cache** and choose one registered source. Do not select
   **All registered sources** when attaching a source-specific banner.
2. Drop an image onto **Source artwork**, or click the drop area to browse.
   Prefer a 1280 × 720 PNG/JPEG (16:9). Inputs are limited to 20 MiB and
   4096 × 4096 pixels; Helpy center-crops a small 256 × 144 RGBA preview.
3. Wait for **Artwork ready**, then choose **Export cached bundle**. The image
   pixels travel inside an owner-scoped `art` record in the `.rsmk-cache` file.
   No original image filename, local path, URL, script or native command is
   needed by the recipient. Failed decoding keeps the previous banner.
4. Include the bundle under `RuneSchema/mods/<ModName>/cache/`. A banner-only
   companion bundle may sit beside the mod's larger item/journal cache bundle.
   The same presentation validation and source revision rules apply.

The item’s actual original provider determines its banner. A modified vanilla
item stays vanilla; a borrowed `/Game` icon is not mod provenance. Shared
namespaces retain separate owner-addressed art records. An unknown provider
gets no inferred banner. `RSDWSpears` is Spears & Scythes; all Dye family
variants use Colorful Dye. Dowdun Reach uses The Bastion artwork across its
whole plugin. The artwork supplied with the initial UI-asset download can be
overridden through the same source-art export controls.

Imported art and local changes live in Helpy's writable AppData cache. The
mod author's supplied bundle is never rewritten. Importing art does not require a live namespace. Verified export still requires
current ownership; an offline author route never registers items.

## Publish a vanilla bundle to GitHub

Capture/export a compatible **core/native-provider-only** bundle first. Review
coverage and redistribution rights for Jagex artwork/text; attribution does
not by itself grant redistribution permission. Never include player saves,
runtime handles, local filesystem paths, network credentials or mod assets
without permission.

Stage an exported bundle with `build/publish-cache-bundle.py`, supplying its
exact game version and runtime. The tool content-addresses the file and updates
`resources/cache-bundles/index.json`; it does not push or publish anything.
Review the staged diff, test import against the declared build, then publish.
The index uses Format 2 / Kind `RuneSchemaCacheBundles`, and each pack declares
Runtime, GameVersion, Locale, Sources, SHA256, Bytes and explicitly partial
record coverage. No entry should claim all vanilla content without an
independent coverage audit.

The index is an updateable data feed; bundle filenames are immutable SHA-256
identities. New entries are placed first while older compatible cook entries
are retained. The client ranks the exact build ahead of cross-build artwork
references, checks byte counts/SHA-256 before import, and keeps its existing
cache on a network or validation failure. UI artwork still uses its separate
compiled, immutable manifest. No DLL, script or gameplay definition is fetched
from this cache feed.

## Map artwork in base-game bundles

The GitHub base caches now include a 2048 × 2048 native map and its coordinate
alignment. Both indexed game versions have a map-bearing download for clients
and dedicated servers. Use the refreshed **0.8.0.2 build from October 27, 2026**
or newer; earlier readers cannot import the map payload.

Helpy's **Download Initial Assets** action also includes this map. In
**Settings → Updates & runtime → Helpy data and artwork**, select UI artwork,
the world map, and/or map filters, then choose **Update selected**. **Update all**
requests every group. Your selections are saved in the normal AppData UI
settings. Map imports reuse the existing cache-bundle validator and install only
the image plus alignment; no server plugin, PAK or graphics capture is required.

The map uses a small, updateable GitHub manifest with an immutable SHA-256
bundle filename. An already installed image/alignment pair is verified and
reused, so subsequent launches need no download. Online checks happen through
these explicit actions, not a timed poll. Cancelling or rejecting an update keeps
the last usable map. Manual map-filter refresh is separate from automatic
refresh after a confirmed game patch.

RSDW Archive publishes raw exported map textures and `.usmap` authoring schemas.
The verified RuneSchema feed currently supplies the paired map/alignment data;
an upstream texture alone does not establish world coordinates. Offline
authoring tools continue to obtain RSDW schemas separately and never install
them into a running game through these asset controls.

To add the map to a server:

1. Open the server's Helpy and choose **Content cache → Download base-game cache**.
   The download includes item references, pictures and the map. Wait for import
   to finish; the map is stored in that platform's AppData cache.
2. Open **World → Map**. The background can load from the downloaded cache;
   the server does not need to render or capture the native map first.
3. Later Helpy sessions reuse the local map. Downloading again is only needed
   when you want a refreshed cache or have removed the local copy.

If you only need the map, download the
[map-only cache](https://raw.githubusercontent.com/gh0sted5456-us/RuneSchema/main/resources/cache-bundles/7c58b7fbf46709e71f490759a620273eb1aaca2406fa66282016519697c8856b.rsmk-cache)
(16 MiB), then use **Content cache → Import** on the server. It contains no item
records, so your existing item cache stays in place. The
[map download manifest](https://raw.githubusercontent.com/gh0sted5456-us/RuneSchema/main/resources/cache-bundles/world-map.json)
provides the filename, byte count and SHA-256 for checking the download.

### Capture your own map

Base-game `.rsmk-cache` exports include the cached world-map image and its exact
map alignment when available. Open **World → Map** in a client world once to
capture the native map. Capture writes to the platform's AppData cache on a file
worker. Export **All sources** or **core** to include it; a mod-only export leaves
the base map out.

Import the resulting bundle on a dedicated server through Helpy's cache window.
The map becomes available from local cache without a client graphics device or
another render-target capture. Later Helpy sessions reuse the same map immediately.
The portable map contains image pixels and coordinate alignment, not players,
chests, placed buildings, character progress or saved filter preferences. Those
world details still come from the running server. Older bundles without a map
remain supported; they cannot supply artwork they did not contain.

The optional `WorldMap` descriptor identifies the fixed native map asset, RSI3
pixel dimensions/byte count, SHA-256 and validated projection. Its image payload
follows the icon payloads. Helpy validates the entire map before publishing a
content-addressed `.rsmap` file and atomically replacing its local `current.json`.
Corrupt, oversized or over-budget input retains the previous map. Map-bearing
bundles require this updated reader; use older bundles with older Helpy builds.

Map snapshots can use cached alignment to display the known Ashenfall world when
the server's native map background is unavailable. That display fallback does not
replace native projection or ground checks used for teleporting or placement.
Static resource/quest reference filters keep their separate map-cache workflow.

## Update-aware mod caches

Helpy keeps using cached items and pictures while it verifies installed content.
The mod version read from `ID.txt` during normal module startup is an extra update
hint; existing legacy metadata precedence is preserved.

- A different version from the saved source manifest queues one background file
  and container verification. Repeated notices for the same version coalesce.
- The version label does not change item ownership, authorize gameplay, clear the
  cache or force pictures to be captured again. Actual source fingerprints still
  decide whether cached content needs updating.
- A version change arriving during verification makes that result outdated;
  Helpy checks the latest version before publishing a new source contract.
- Unchanged ownership indexes are retained, with duplicate comparisons and
  cleanup on the existing file worker rather than the game/UI thread.
- This uses startup metadata and the existing source-refresh schedule. It adds
  no per-frame `ID.txt` reads, directory watcher or network polling. Directly
  editing `ID.txt` during a running session is picked up at the next module
  startup, just like the mod's newly installed PAKs.

Keep the internal ModID stable and update `Version` when publishing a mod. A
missing version remains supported, and an unchanged version is never proof that
the PAKs match. Existing file and source checks still apply.

## Export the cache you already have

The cache window follows **Add cached content → Fill missing content (optional)
→ Save current cache**. **Export current cache...** works without running Build
first or waiting for live ownership verification. It includes downloaded library
records/pictures, learned local references and the map when available. A game
update or changed PAK inventory does not invalidate this saved snapshot.
Build / update remains the optional way to discover more content before export.

Snapshot fingerprints describe cached records and picture checksums, independent
of installation/folder names. These bundles carry `PresentationOnly: true` and
partial coverage, so a reader cannot mistake them for a verified current native
registry. Helpy imports them as display references and filters unavailable mod
sources during browsing. Gameplay actions still validate live. Author builds
that verify inactive PAK files retain their separate verification path.
