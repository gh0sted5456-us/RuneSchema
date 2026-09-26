# Frequently asked questions

These are the cross-cutting RuneSchema questions that do not belong to only one
loader. Loader-specific questions remain on each loader page and are also
searchable through [Ask RuneSchema](ASK-RUNESCHEMA.md).

## General authoring

### FAQ-GENERAL-001 — Where do RuneSchema mod files go? {#faq-general-001}

Put loader files under:

```text
RuneSchema/mods/<ModName>/<loader>/
```

For example, item definitions belong under `assets/`, DataTable work under
`raw/`, and recipe definitions under `recipes/`.

### FAQ-GENERAL-002 — Do I need to repeat modId in every JSON file? {#faq-general-002}

No. The mod folder name is the owner namespace for normal loader authoring.
Use the loader's documented JSON shape instead of repeating ownership metadata
unless that specific format requires it.

### FAQ-GENERAL-003 — Can I organize loader files in nested folders? {#faq-general-003}

Yes. Loader directories are recursive for `.json` and `.jsonc` files.

### FAQ-GENERAL-004 — Do nested folder names become part of runtime identity? {#faq-general-004}

No. Nested folders are organizational. Runtime identity comes from the object
path, row name, loader ID, `PersistenceID`, or other identity defined by that
loader.

### FAQ-GENERAL-005 — Can I use JSONC comments? {#faq-general-005}

Yes. RuneSchema accepts both `.json` and `.jsonc`, and JSONC may contain
comments.

### FAQ-GENERAL-006 — Does one bad file disable the whole mod? {#faq-general-006}

Not normally. RuneSchema isolates loader, mod, and section failures where it is
safe to continue. Fix the first warning or error for the affected loader, then
retest.

### FAQ-GENERAL-007 — Do server and clients need the same cooked assets? {#faq-general-007}

Yes for content that clients must render. Gameplay decisions remain
server-authoritative, but a client still needs the cooked item, mesh, Blueprint,
Niagara system, icon, or other asset it is expected to display.

### FAQ-GENERAL-008 — Is a USMAP required? {#faq-general-008}

No. USMAP data is optional and helps type queries and diagnostics. Live Unreal
reflection still validates runtime writes.

### FAQ-GENERAL-009 — Can RuneSchema JSON create a completely new Unreal class? {#faq-general-009}

No general loader creates new Unreal classes from JSON. Cook new Blueprint or
other Unreal classes into a PAK, then use RuneSchema to register, reference, or
configure the supported content.

### FAQ-GENERAL-010 — Is Helpy required for RuneSchema mods to work? {#faq-general-010}

No. Helpy is optional. Core loaders do not depend on it.

### FAQ-GENERAL-011 — Should I use short names or full cooked object paths? {#faq-general-011}

Follow the selected loader's contract. Use a full cooked path when the loader
supports exact targeting and a short name could be ambiguous.

### FAQ-GENERAL-012 — Can RuneSchema edit vanilla content? {#faq-general-012}

Yes where a loader explicitly supports patching an existing loaded object, row,
course, or other record. RuneSchema does not provide unrestricted raw-memory
mutation as a fallback.

### FAQ-GENERAL-013 — Why should a PersistenceID stay stable? {#faq-general-013}

Persistent content uses that identity across saves and updates. Do not reuse a
vanilla or another mod's persistence identity, and do not casually change a
published ID.

### FAQ-GENERAL-014 — What happens when a RuneSchema mod is removed? {#faq-general-014}

SafeSave considers only persistent identities RuneSchema previously recorded as
owned by that mod. Unknown, vanilla, and third-party identities are not treated
as removable just because they fail to resolve.

### FAQ-GENERAL-015 — Can I copy an example without changing anything? {#faq-general-015}

Treat examples as working authoring patterns. Replace mod paths, IDs,
`PersistenceID` values, DataTable rows, and cooked asset references with the
ones that belong to your mod.

## Choosing a loader

### FAQ-GENERAL-016 — Which loader should I use for a DataTable row? {#faq-general-016}

Use `/raw`.

### FAQ-GENERAL-017 — Which loader should I use for an item or DataAsset? {#faq-general-017}

Use `/assets`.

### FAQ-GENERAL-018 — Which loader should I use for an existing Blueprint class default? {#faq-general-018}

Use `/blueprints` for the supported reflected defaults documented by that
loader.

### FAQ-GENERAL-019 — Which loader handles multiplayer action/presentation declarations? {#faq-general-019}

Use `/registry`. It complements the normal content loaders; it does not
replace them.
