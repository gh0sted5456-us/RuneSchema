# Shared content cache bundles

Helpy can import/export a `.rsmk-cache` presentation bundle: exact Unreal
asset paths, known persistence IDs, readable names, source-owner routing,
item metadata, journal text and artwork references, plus cached preview pixels.
It does **not** install mods or deserialize executable loader definitions.

!!! note "Availability"
    This is the experimental bundle contract. The existing published base-game
    download is a **partial, legacy icons-only pack**. A complete vanilla
    item/journal bundle has not yet been captured and published. An empty
    version-2 index is intentional; downloads fall back to a compatible legacy
    pack, otherwise local discovery remains available.

## Use an existing cache immediately

1. Open **Icon cache** and choose **Import**, or **Download base-game cache**.
2. Import verifies the whole file, portable content fingerprints and owner
   scope on a file worker. Unrelated or missing sources are skipped.
3. Matching records enter an owner/category cache. Item Bay search can use
   those names, paths and IDs without a fresh icon sweep. Journal references
   and portrait artwork become available to their relevant consumers.
4. Existing bounded runtime discovery checks current content in the background.
   Cached presentation is not proof of current-world presence, unlock/read
   state, spawnability or delivery. Gameplay actions still resolve live assets.

The first file read is asynchronous, not literally zero-time. A cached result
is labelled a reference while verification continues. Empty partial bundles
must not replace a complete native catalogue or prove that a search has no
other matches. Focused live lookup remains the fallback after a catalogue miss.

When a mod changes, its old presentation references can remain searchable while
checking changes; removed/inactive owners are not imported as new content.
Current source identities control verified exports. Missing or stale values
are unknown, never invented persistence IDs or zero player readings.

## Export and share

**Export cached bundle** does not require a full sweep. It exports currently
compatible cached item/journal records and indexed pictures. **Build / update**
still performs the existing full icon-discovery/capture workflow. Advanced
**Build missing icons, then export** runs that workflow before exporting.

A full icon survey is not a full item/journal dump. Record coverage remains
explicitly partial. Missing persistence IDs are exported as an empty string
with `PersistenceIDKnown: false`; they are not replaced by an asset name.

Mod authors may include `cache/*.rsmk-cache` inside their RuneSchema mod folder.
Automatic imports accept typed metadata only for that containing, active mod's
declared content namespace. Borrowed vanilla/plugin artwork does not establish
item ownership. A patched vanilla item remains vanilla; modifying it is not a
new original-provider claim. Ambiguous native-root clones are conservatively
excluded from mod metadata export.

When several mods share one mounted namespace (such as Dye and its trim
components), references can be routed to the declared owner without treating
that declaration as original-provider proof. A source badge requires exact-path
native/authoring evidence or an unambiguous provider. Native item attribution
repairs the presentation while the original cache-owner routing remains intact.

Legacy `.rsmk-icons` files still import/export preview pixels. They contain no
item identities, persistence IDs or journal text.

## Who owns the files?

Mod-supplied files are **read-only inputs**. Helpy never writes an updated bundle
back into an installed mod. Local captures, repaired metadata and verification
results belong to Helpy's own writable AppData cache.

| Location | Role |
| --- | --- |
| `RuneSchema/mods/<owner>/cache/*.rsmk-cache` | Author-distributed input bundle; never rewritten |
| `%LOCALAPPDATA%/RuneSchema/HELPY/runtime/saved/cache/item-icons` | Imported/captured thumbnail files and source snapshot |
| `…/cache/content-bundles/<install-key>` | Typed owner/category metadata and atomically committed local index |
| `…/cache/item-catalogues` | Verified/saved native item catalogue used alongside imported references |
| `…/cache/journal` | Local native journal references and observed text/artwork |

An author releases a replacement bundle with a mod update. Helpy checks active
content compatibility and merges usable records into its own storage. To share
local improvements, the user explicitly exports a new bundle and gives it to
the author; that is not an automatic edit or upload.

For a family of separately installed mods, ship each owner-specific bundle in
that owner's `cache` folder. Automatic import will not let a bundle under one
mod populate another mod's metadata. A combined family bundle can instead be
imported explicitly through Helpy; all referenced owners must be active and
compatible.

Pure base-game bundles match the installed native cook independently of optional
mods. With mods enabled, their pictures remain baseline presentation references,
not proof of a mod's overridden artwork; they cannot be re-exported as
mod-verified captures. Typed paths/IDs still undergo owner routing and targeted
native gameplay validation.

## Bundle format

| Field | Meaning |
| --- | --- |
| File header | Eight ASCII bytes `RSCACHE1`, little-endian uint32 manifest length |
| JSON manifest | Version 2, Kind `RuneSchemaCacheBundle`, Locale, Sources, Records, Icons |
| Sources | Asset-source group → portable SHA-256 of native content and contributing owners |
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

- Verification uses content bytes, not storefront executable hashes or local
  timestamps. Runtime/game version and locale narrow GitHub download candidates;
  actual import independently checks portable fingerprints.
- Imports are explicit for GitHub; there is no background network sweep.
  Active mod-provided bundles can be imported locally in the background.
- All blobs are checked before publication. Typed records are split by owner,
  category and source; immutable metadata files are committed through an atomic
  index update. Failure/cancellation keeps the previous index.
- Partial updates merge, not erase other owners or categories. Reimporting
  identical content does not rewrite matching records or pictures.
- Limits: 512 MiB total bundle, 80 MiB manifest, 64 MiB / 65,536 metadata records,
  32,768 pictures, 256×256 maximum thumbnail size. Native journal presentation
  has its own 8,192-entry / 12 MiB UI limit.
- Disk work, JSON parsing and source hashing stay off the rendering/game threads.
  Item bundle workers check the index periodically and reuse unchanged files.
- Server display-only imports remain **pictures only**; mismatched definitions
  or metadata cannot become content-verified through that option.

Useful diagnostics include [RS-CACHE-002](ERROR-CODES.md#rs-cache-002)
(invalid bundle), [RS-CACHE-003](ERROR-CODES.md#rs-cache-003) (identity mismatch),
[RS-CACHE-004](ERROR-CODES.md#rs-cache-004) (integrity),
[RS-CACHE-008](ERROR-CODES.md#rs-cache-008) (unproven owner) and
[RS-CACHE-010](ERROR-CODES.md#rs-cache-010) (legacy pixels only).

## Include a source banner

1. Open Helpy's **Icon cache** and choose one registered source. Do not select
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
   Source fingerprints and active-owner restrictions still apply on import.

The item’s actual original provider determines its banner. A modified vanilla
item stays vanilla; a borrowed `/Game` icon is not mod provenance. Shared
namespaces retain separate owner-addressed art records. An unknown provider
gets no inferred banner. `RSDWSpears` is Spears & Scythes; all Dye family
variants use Colorful Dye. Dowdun Reach uses The Bastion artwork across its
whole plugin. The artwork supplied with the initial UI-asset download can be
overridden through the same source-art export controls.

Imported art and local changes live in Helpy's writable AppData cache. The
mod author's supplied bundle is never rewritten. A source must have an active
owned namespace before a typed art record can be imported/exported; an author
route in an offline companion pack does not grant ownership or register items.

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
