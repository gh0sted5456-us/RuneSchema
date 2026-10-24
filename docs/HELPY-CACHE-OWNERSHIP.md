# Per-mod cache ownership

Source IDs are canonical lowercase folder IDs, for example `mods/rsdwspears`,
not display nicknames. A shared Unreal mount is not proof that one mod owns
all its assets.

## Container-first discovery

Shared state lives under `%LOCALAPPDATA%/RSDragonwilds/RuneSchema/core`;
`steam`, `gamepass`, `epic` and `server` hold their runtime-specific state and
mapping caches. Startup creates missing managed directories on demand without
copying, moving or deleting legacy runtime/cache data. Old install-local
`runtime/live` directories and older AppData roots are not imported or scanned.
Existing flat `HELPY`, `runeschema.profile` and `health` folders inside the current
AppData root remain in use if their `core` counterpart does not exist. If both
exist, the core folder wins; neither is merged or overwritten.
Links inside managed data remain forbidden.

To reset generated caches, close all games/Helpys that use them and remove only
the cache folder shown by **Open cache folder**. Missing cache data is rebuilt.
Do not delete profile, loadout, progress or Recovery files as a cache reset.

In the cache window, choose a mod source and **Share selected mod cache in
core** to publish its current verified bundle to
`core/cache/mods/<owner>/shared.rsmk-cache`. Other runtimes discover this alongside
packaged mod caches on their next content-source scan/launch. Matching portable
content hashes and current exact ownership are still required; mismatches are
skipped. This is a snapshot, not automatic recapture, and sharing does not mount
content or make cached items available. Each mod remains independently scoped.

The source worker reads the bounded directory indexes of modern PAK v10/v11
and unencrypted, indexed IoStore v3–v8 containers. UCAS payloads are not
extracted. Startup read directories retain their RuneSchema mod/plugin owner,
including enabled PAK-only mods without registry declarations. Disabled
managed mod folders are excluded. Native content containers are inspected too.

Exact package names are mapped to their supplying logical container filenames
and owner tokens. That evidence takes priority over package-prefix guesses,
including content placed directly under /Game. Multiple owners remain
ambiguous: discovery does not guess mount precedence. Item details show the
supplying filenames; the cache window's Advanced section lists indexed and
unavailable containers and explains unavailable indexes.

Encrypted, pruned, unsupported, malformed or changing indexes publish no
partial ownership. PAK indexes must pass their native SHA-1 checks. Bounded
metadata hashes join the existing platform/content fingerprints, so directory
changes invalidate the affected cache identities even at the same size/time.
This does not hash every payload byte or certify container signatures.

An indexed file is a **supplier candidate**, not proof that Unreal mounted it
successfully. Actual catalog items and gameplay actions still require native
discovery/validation. These cache associations never authorize save pruning,
registration, item delivery or arbitrary asset loading.

Container package maps stay process-local. Cache import/export requests exact
provider tokens in revision-bound read-only pages; no package map is broadcast
in each Helpy heartbeat. [CACHE][PAK-DISCOVERY] reports summary counts, with
unavailable-index details at verbose level.

Both loose `registry/*.jsonc` and cooked registry declarations use the same
parser, validation limits and ownership publication after successful acceptance.
Use the normal typed lanes for actual items, recipes and journal entries.
To announce original artwork without creating a gameplay registration:

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "CacheAssets": [
      "/Game/Mods/MyMod/UI/T_Original.T_Original"
    ]
  }
}
```

Each lane permits 4,096 unique paths; each document permits 16,384 total
references. Split larger declarations across bounded files or cooked shards.
Only declare exact original assets. Borrowed vanilla/official-plugin icons
remain owned by their native source and can accompany the exact referencing
item under the existing borrowed-art rules.

The host sends exact ownership to Helpy through bounded read-only pages, not
the entire asset map in each catalog heartbeat. Each page is tied to the source
fingerprint and content revision. Conflicting owners remain conflicting;
changed or unavailable proofs fail the operation without changing the last-good
cache. These proofs apply to manual source exports/imports and packaged
`mods/<mod>/cache/*.rsmk-cache` imports. They do not grant gameplay authority,
load missing mods, or fix broken cooked assets.

This experimental contract requires the matching core and Helpy build in the
0.8.0.1 refresh. Automated checks are not live certification of every source,
platform or cache consumer; ownership never becomes gameplay authority.
