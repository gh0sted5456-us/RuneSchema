# Per-mod cache ownership

Source IDs are canonical lowercase folder IDs, for example `mods/rsdwspears`,
not display nicknames. A shared Unreal mount is not proof that one mod owns
all its assets.

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

This contract is staged source work; it requires the matching core and Helpy
build, and has not yet been installed/live-verified.
